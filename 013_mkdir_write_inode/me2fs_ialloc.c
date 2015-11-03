/********************************************************************************
	File			: me2fs_ialloc.c
	Description		: Allocating Operations of my ext2 file system

*********************************************************************************/
#include <linux/backing-dev.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"
#include "me2fs_block.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static long
findDirectoryGroup( struct super_block *sb, struct inode *parent );
struct buffer_head*
readInodeBitmap( struct super_block *sb, unsigned long block_group );
static void prereadInode( struct inode *inode );

/*
==================================================================================

	DEFINES

==================================================================================
*/

/*
==================================================================================

	Management

==================================================================================
*/

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsAllocNewInode
	Input		:struct inode *dir
				 < vfs inode of directory >
				 umode_t mode
				 < file mode >
				 const struct qstr *qstr
				 < entry name for new inode >
	Output		:void
	Return		:struct inode*
				 < new allocated inode >

	Description	:allocate new inode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct inode*
me2fsAllocNewInode( struct inode *dir, umode_t mode, const struct qstr *qstr )
{
	struct super_block		*sb;
	struct buffer_head		*bitmap_bh;
	struct buffer_head		*bh_gdesc;

	struct inode			*inode;			/* new inode */
	ino_t					ino;
	struct ext2_group_desc	*gdesc;
	struct ext2_super_block	*esb;
	struct me2fs_inode_info	*mi;
	struct me2fs_sb_info	*msi;

	unsigned long			group;
	int						i;
	int						err;

	/* ------------------------------------------------------------------------ */
	/* allocate vfs new inode													*/
	/* ------------------------------------------------------------------------ */
	sb = dir->i_sb;

	if( !( inode = new_inode( sb ) ) )
	{
		return( ERR_PTR( -ENOMEM ) );
	}

	bitmap_bh	= NULL;
	ino			= 0;

	msi			= ME2FS_SB( sb );

	if( S_ISDIR( mode ) )
	{
		group = findDirectoryGroup( sb, dir );
	}
	else
	{
		/* -------------------------------------------------------------------- */
		/* as for now allocating inode for file is not support					*/
		/* -------------------------------------------------------------------- */
		err = -ENOSPC;
		goto fail;
	}

	DBGPRINT( "(ME2FS)alloc inode : group = %lu\n", group );

	if( group == -1 )
	{
		err = -ENOSPC;
		goto fail;
	}

	for( i = 0 ; i < msi->s_groups_count ; i++ )
	{
		brelse( bitmap_bh );
		if( !( bitmap_bh = readInodeBitmap( sb, group ) ) )
		{
			err = -EIO;
			goto fail;
		}

		ino = 0;

		/* -------------------------------------------------------------------- */
		/* find free inode														*/
		/* -------------------------------------------------------------------- */
repeat_in_this_group:
		ino = find_next_zero_bit_le( ( unsigned long* )bitmap_bh->b_data,
									 msi->s_inodes_per_group,
									 ino );

		if( ME2FS_SB( sb )->s_inodes_per_group <= ino )
		{
			/* cannot find ino. bitmap is already full							*/
			group++;

			if( group <= msi->s_groups_count )
			{
				group = 0;
			}

			continue;
		}

		/* -------------------------------------------------------------------- */
		/* allocate inode atomically											*/
		/* -------------------------------------------------------------------- */
		if( ext2_set_bit_atomic( getSbBlockGroupLock( msi, group ),
								 ( int )ino,
								 bitmap_bh->b_data ) )
		{
			/* ---------------------------------------------------------------- */
			/* already set the bitmap											*/
			/* ---------------------------------------------------------------- */
			ino++;
			if( msi->s_inodes_per_group <= ino )
			{
				/* the group has no entry, try next								*/
				group++;
				if( msi->s_groups_count <= group )
				{
					group = 0;
				}
				continue;
			}

			/* try to find in the same group									*/
			goto repeat_in_this_group;
		}

		goto got;
	}

	/* ------------------------------------------------------------------------ */
	/* cannot find free inode													*/
	/* ------------------------------------------------------------------------ */
	err = -ENOSPC;
	goto fail;

	/* ------------------------------------------------------------------------ */
	/* found free inode															*/
	/* ------------------------------------------------------------------------ */
got:
	mi		= ME2FS_I( inode );
	esb		= msi->s_esb;

	mark_buffer_dirty( bitmap_bh );
	if( sb->s_flags & MS_SYNCHRONOUS )
	{
		sync_dirty_buffer( bitmap_bh );
	}
	brelse( bitmap_bh );

	/* ------------------------------------------------------------------------ */
	/* get absolute inode number												*/
	/* ------------------------------------------------------------------------ */
	ino	+= ( group * ME2FS_SB( sb )->s_inodes_per_group ) + 1;

	if( ( ino < msi->s_first_ino ) ||
		( le32_to_cpu( esb->s_inodes_count ) < ino ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:insane inode number. ino=%lu,group=%lu\n",
					  __func__, ( unsigned long )ino, group );
		err = -EIO;
		goto fail;
	}

	/* ------------------------------------------------------------------------ */
	/* update group descriptor													*/
	/* ------------------------------------------------------------------------ */
	gdesc		= me2fsGetGroupDescriptor( sb, group );
	bh_gdesc	= me2fsGetGdescBufferCache( sb, group );

	percpu_counter_add( &msi->s_freeinodes_counter, -1 );

	if( S_ISDIR( mode ) )
	{
		percpu_counter_inc( &msi->s_dirs_counter );
	}

	spin_lock( getSbBlockGroupLock( msi, group ) );
	{
		le16_add_cpu( &gdesc->bg_free_inodes_count, -1 );
		if( S_ISDIR( mode ) )
		{
			le16_add_cpu( &gdesc->bg_used_dirs_count, 1 );
		}
	}
	spin_unlock( getSbBlockGroupLock( msi, group ) );

	mark_buffer_dirty( bh_gdesc );

	/* ------------------------------------------------------------------------ */
	/* initialize vfs inode														*/
	/* ------------------------------------------------------------------------ */
	inode_init_owner( inode, dir, mode );

	inode->i_ino	= ino;
	inode->i_blocks	= 0;
	inode->i_mtime	= CURRENT_TIME_SEC;
	inode->i_atime	= inode->i_mtime;
	inode->i_ctime	= inode->i_mtime;

	/* ------------------------------------------------------------------------ */
	/* initialize me2fs inode information										*/
	/* ------------------------------------------------------------------------ */
	memset( mi->i_data, 0, sizeof( mi->i_data ) );

	mi->i_flags = ME2FS_I( dir )->i_flags & EXT2_FL_INHERITED;

	if( S_ISDIR( mode ) )
	{
		/* do nothing															*/
	}
	else if( S_ISREG( mode ) )
	{
		mi->i_flags &= EXT2_REG_FLMASK;
	}
	else
	{
		mi->i_flags &= EXT2_OTHER_FLMASK;
	}

	mi->i_faddr				= 0;
	mi->i_frag_no			= 0;
	mi->i_frag_size			= 0;
	mi->i_file_acl			= 0;
	mi->i_dir_acl			= 0;
	mi->i_dtime				= 0;
	//mi->i_block_alloc_info	= NULL;
	mi->i_state				= EXT2_STATE_NEW;

	me2fsSetVfsInodeFlags( inode );
	
	/* insert vfs inode to hash table											*/
	if( insert_inode_locked( inode ) < 0 )
	{
		ME2FS_ERROR( "<ME2FS>%s:inode number already in use[%lu]\n",
					 __func__, ( unsigned long )ino );
		err = -EIO;
		goto fail;
	}

	/* initialize quota															*/
#if 0	// quota
	dquot_initialize( inode );
	if( dquot_alloc_inode( inode ) )
	{
		goto fail_drop;
	}
#endif
#if 0	// acl
	/* initialize acl															*/
	if( me2fsInitAcl( inde, dir ) )
	{
		goto fail_free_drop;
	}
#endif
#if 0	// security
	/* initialize security														*/
	if( me2fsInitSecurity( inode, dir, qstr ) )
	{
		goto fail_free_drop;
	}
#endif

	mark_inode_dirty( inode );

	prereadInode( inode );

	DBGPRINT( "<ME2FS>allocating new inode %lu\n",
			  ( unsigned long )inode->i_ino );

	DBGPRINT( "<ME2FS>%s\n", __func__ );
	dbgPrintVfsInode( inode );
	dbgPrintMe2fsInodeInfo( ME2FS_I( inode ) );

	return( inode );

	/* ------------------------------------------------------------------------ */
	/* allocation of new inode is failed										*/
	/* ------------------------------------------------------------------------ */
fail:
	make_bad_inode( inode );
	iput( inode );
	return( ERR_PTR( err ) );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsCountFreeInodes
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:unsigned long
				 < number of free inodes >

	Description	:count number of free inodes in the file system
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
unsigned long me2fsCountFreeInodes( struct super_block *sb )
{
	unsigned long	freei;
	int				group;

	freei = 0;

	for( group = 0 ; group < ME2FS_SB( sb )->s_groups_count ; group++ )
	{
		struct ext2_group_desc	*cur_desc;
		
		cur_desc = me2fsGetGroupDescriptor( sb, group );

		if( !cur_desc )
		{
			continue;
		}

		freei += le16_to_cpu( cur_desc->bg_free_inodes_count );
	}

	return( freei );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsCountDirectories
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:unsigned long
				 < number of directories in file system >

	Description	:count number of directories in file system
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
unsigned long me2fsCountDirectories( struct super_block *sb )
{
	unsigned long	dir_count;
	int				i;

	dir_count = 0;

	for( i = 0 ; i < ME2FS_SB( sb )->s_groups_count ; i++ )
	{
		struct ext2_group_desc	*gdesc;

		if( !( gdesc = me2fsGetGroupDescriptor( sb, i ) ) )
		{
			continue;
		}

		dir_count += le16_to_cpu( gdesc->bg_used_dirs_count );
	}

	return( dir_count );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Local Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
==================================================================================
	Function	:findDirectoryGroup
	Input		:struct super_block *sb
				 < vfs super block >
				 struct inode *parent
				 < parent directory >
	Output		:void
	Return		:unsigned long
				 < group number for allocating blocks to directory >

	Description	:find a group for allocating blocks to directory
==================================================================================
*/
static long
findDirectoryGroup( struct super_block *sb, struct inode *parent )
{
	struct ext2_group_desc	*best_desc;
	unsigned long			groups_count;
	unsigned long			avefreei;
	int						group;
	int						best_group;

	best_desc		= NULL;
	groups_count	= ME2FS_SB( sb )->s_groups_count;
	avefreei		= me2fsCountFreeInodes( sb ) / groups_count;
	best_group		= -1;
	
	for( group = 0 ; group < groups_count ; group++ )
	{
		struct ext2_group_desc	*cur_desc;

		cur_desc = me2fsGetGroupDescriptor( sb, group );

		/* -------------------------------------------------------------------- */
		/* if there is no available inode or invalid descriptor, go next		*/
		/* -------------------------------------------------------------------- */
		if( !cur_desc || !cur_desc->bg_free_inodes_count )
		{
			continue;
		}
		/* -------------------------------------------------------------------- */
		/* if number of inodes is less than average, go next					*/
		/* -------------------------------------------------------------------- */
		if( le16_to_cpu( cur_desc->bg_free_inodes_count ) < avefreei )
		{
			continue;
		}
		
		if( !best_desc ||
			( le16_to_cpu( best_desc->bg_free_blocks_count	) <
			  le16_to_cpu( cur_desc->bg_free_blocks_count	) ) )
		{
			/* ---------------------------------------------------------------- */
			/* found candidate													*/
			/* ---------------------------------------------------------------- */
			best_group	= group;
			best_desc	= cur_desc;
		}
	}

	return( best_group );
}
/*
==================================================================================
	Function	:readInodeBitmap
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned long block_group
				 < block group number >
	Output		:void
	Return		:strut buffer_head
				 < buffer cache on which inode bitmap resides

	Description	:read inode bitmap in block_group'th block group
==================================================================================
*/
struct buffer_head*
readInodeBitmap( struct super_block *sb, unsigned long block_group )
{
	struct ext2_group_desc	*gdesc;
	struct buffer_head		*bh;

	if( !( gdesc = me2fsGetGroupDescriptor( sb, block_group ) ) )
	{
		return( NULL );
	}
	
	if( !( bh = sb_bread( sb, le32_to_cpu( gdesc->bg_inode_bitmap ) ) ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:can not read inode bitmap\n", __func__ );
		ME2FS_ERROR( "<ME2FS>block_group = %lu, inode_bitmap = %u\n",
					  block_group, le32_to_cpu( gdesc->bg_inode_bitmap ) );
	}

	return( bh );
}
/*
==================================================================================
	Function	:prereadInode
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:preread the new inode's inode block when creating the inode
==================================================================================
*/
static void prereadInode( struct inode *inode )
{
	unsigned long			block_group;
	unsigned long			offset;
	unsigned long			block;
	unsigned long			inodes_per_group;
	struct ext2_group_desc	*gdesc;
	struct backing_dev_info	*bdi;
/*
	// from kernel 4.0 or later address_space does not have a backing_dev_info
	// so, use inode_to_bdi interface.
	bdi = inode_to_bdi( inode );
*/
	// address_space has a backing_dev_info in kernel 3.9 or before
	bdi = inode->i_mapping->backing_dev_info;

	if( bdi_read_congested( bdi ) )
	{
		return;
	}

	if( bdi_write_congested( bdi ) )
	{
		return;
	}

	inodes_per_group	= ME2FS_SB( inode->i_sb )->s_inodes_per_group;
	block_group			= ( inode->i_ino - 1 ) / inodes_per_group;
	gdesc				= me2fsGetGroupDescriptor( inode->i_sb, block_group );

	if( gdesc == NULL )
	{
		return;
	}
	
	/* ------------------------------------------------------------------------ */
	/* figure out the offset within the block group inode table					*/
	/* ------------------------------------------------------------------------ */
	offset	= ( ( inode->i_ino - 1 ) % inodes_per_group ) *
			  ME2FS_SB( inode->i_sb )->s_inode_size;
	block	= le32_to_cpu( gdesc->bg_inode_table ) +
			  ( offset >> inode->i_sb->s_blocksize_bits );
	
	sb_breadahead( inode->i_sb, block );
}
/*
==================================================================================
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
