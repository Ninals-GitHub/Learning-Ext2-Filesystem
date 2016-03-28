/********************************************************************************
	File			: me2fs_block.c
	Description		: management blocks of my ext2 file system

*********************************************************************************/
#include <linux/sched.h>
#include <linux/buffer_head.h>
#include <linux/capability.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_block.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static int isGroupSparse( int group );
static int testRoot( int group, int multiple );
struct ext2_reserve_window;

static int hasFreeBlocks( struct me2fs_sb_info *msi );
static struct buffer_head*
readBlockBitmap( struct super_block *sb, unsigned long block_group );
static int
validBlockBitmap( struct super_block *sb,
				  struct ext2_group_desc *gdesc,
				  unsigned long block_group,
				  struct buffer_head *bh );
static int
tryToAllocate( struct super_block *sb,
			   unsigned long group,
			   struct buffer_head *bitmap_bh,
			   unsigned long grp_goal,
			   unsigned long *count,
			   struct ext2_reserve_window *my_rsv );
static void
adjustGroupBlocks( struct super_block *sb,
				   unsigned long group_no,
				   struct ext2_group_desc *gdesc,
				   struct buffer_head *bh,
				   long count );

/*
==================================================================================

	DEFINES

==================================================================================
*/
#define	IN_RANGE( b, first, len )	( ( ( first ) <= ( b ) )					\
									  && ( ( b ) <= ( first ) + ( len ) + 1 ) )

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
	Function	:me2fsGetGroupDescriptor
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int block_group
				 < block group number >
	Output		:void
	Return		:struct ext2_group_desc*
				 < goup descriptor >

	Description	:get ext2 group descriptor
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct ext2_group_desc*
me2fsGetGroupDescriptor( struct super_block *sb,
						unsigned int block_group )
{
	struct me2fs_sb_info	*msi;
	unsigned long			gdesc_index;
	unsigned long			gdesc_offset;
	struct ext2_group_desc	*group_desc;

	msi = ME2FS_SB( sb );

	if( msi->s_groups_count <= block_group )
	{
		ME2FS_ERROR( "<ME2FS>block group number is out of groups count of sb\n" );
		ME2FS_ERROR( "<ME2FS>s_groups_count = %lu\n", msi->s_groups_count );
		ME2FS_ERROR( "<ME2FS>block_group = %u\n", block_group );
		return( NULL );
	}

	gdesc_index		= block_group / msi->s_desc_per_block;

	if( !( group_desc =
		( struct ext2_group_desc* )( msi->s_group_desc[ gdesc_index ]->b_data )  ) )
	{
		ME2FS_ERROR( "<ME2FS>canot find %u th group descriptor\n", block_group );
		return( NULL );
	}

	gdesc_offset	= block_group % msi->s_desc_per_block;

	//return( &group_desc[ gdesc_offset ] );
	return( group_desc + gdesc_offset );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetGdescBufferCache
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int block group
				 < block group number >
	Output		:void
	Return		:struct buffer_head*
				 < buffer cache to which the block descriptor belongs >

	Description	:get buffer cache the block descriptor belongs to
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct buffer_head*
me2fsGetGdescBufferCache( struct super_block *sb, unsigned int block_group )
{
	struct me2fs_sb_info	*msi;
	unsigned long			gdesc_index;

	msi = ME2FS_SB( sb );

	if( msi->s_groups_count <= block_group )
	{
		ME2FS_ERROR( "<ME2FS>%s:large block group number", __func__ );
		ME2FS_ERROR( "<ME2FS>block_group = %u, s_groups_count = %lu\n",
					  block_group, msi->s_groups_count );
		return( NULL );
	}

	gdesc_index = block_group / msi->s_desc_per_block;

	return( msi->s_group_desc[ gdesc_index ] );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsHasBgSuper
	Input		:struct super_block *sb
				 < vfs super block >
				 int gourp
				 < group number >
	Output		:void
	Return		:int
				 < result >

	Description	:does block goup have super block?
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsHasBgSuper( struct super_block *sb, int group )
{
	if( ( ME2FS_SB( sb )->s_esb->s_feature_ro_compat &
		  EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER ) &&
		( !isGroupSparse( group ) ) )
	{
		return( 0 );
	}

	return( 1 );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsCountFreeBlocks
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:unsigned long
				 < number of free blocks in file system >

	Description	:count number of free blocks in file system
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
unsigned long me2fsCountFreeBlocks( struct super_block *sb )
{
	unsigned long	desc_count;
	int				i;

	desc_count = 0;
	for( i = 0 ; i < ME2FS_SB( sb )->s_groups_count ; i++ )
	{
		struct ext2_group_desc	*gdesc;

		if( !( gdesc = me2fsGetGroupDescriptor( sb, i ) ) )
		{
			continue;
		}
		desc_count += le16_to_cpu( gdesc->bg_free_blocks_count );
	}

	return( desc_count );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsNewBlocks
	Input		:struct inode *inode
				 < vfs inode >
				 unsigned long goal
				 < block number of goal >
				 unsinged long *count
				 < number of allocating block >
				 int *err
				 < result >
	Output		:int *err
				 < result >
	Return		:unsigned long
				 < filesystem-wide allocated block >

	Description	:core block(s) allocation function
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
unsigned long
me2fsNewBlocks( struct inode *inode,
				unsigned goal,
				unsigned long *count,
				int*err )
{
	struct super_block		*sb;
	struct me2fs_sb_info	*msi;
	struct ext2_super_block	*esb;
	struct ext2_group_desc	*gdesc;

	struct buffer_head		*bitmap_bh;
	struct buffer_head		*gdesc_bh;

	unsigned long			group_no;
	unsigned long			goal_group;
	unsigned long			free_blocks;
	unsigned long			grp_alloc_blk;
	unsigned long			grp_target_blk;
	unsigned long			ret_block;
	unsigned long			num;
	int						performed_allocation;

	sb			= inode->i_sb;
	msi			= ME2FS_SB( sb );
	esb			= msi->s_esb;

	bitmap_bh	= NULL;

	ret_block	= 0;
	num			= *count;
	
	performed_allocation = 0;

	if( !hasFreeBlocks( msi ) )
	{
		*err = -ENOSPC;
		goto out;
	}
	/* ------------------------------------------------------------------------ */
	/* test whether the goal block is free										*/
	/* ------------------------------------------------------------------------ */
	if( ( goal < le32_to_cpu( esb->s_first_data_block ) )
		|| le32_to_cpu( esb->s_blocks_count ) <= goal )
	{
		goal = le32_to_cpu( esb->s_first_data_block );
	}

	group_no	= ( goal - le32_to_cpu( esb->s_first_data_block ) )
				  / msi->s_blocks_per_group;
	goal_group	= group_no;

	if( !( gdesc = me2fsGetGroupDescriptor( sb, group_no ) ) )
	{
		goto io_error;
	}

	if( !( gdesc_bh = me2fsGetGdescBufferCache( sb, group_no ) ) )
	{
		goto io_error;
	}

	free_blocks = le16_to_cpu( gdesc->bg_free_blocks_count );

	if( 0 < free_blocks )
	{
		grp_target_blk	= ( goal - le32_to_cpu( esb->s_first_data_block ) )
						  % msi->s_blocks_per_group;
		bitmap_bh		= readBlockBitmap( sb, group_no );

		if( !bitmap_bh )
		{
			goto io_error;
		}

		grp_alloc_blk = tryToAllocate( sb,
									   group_no,
									   bitmap_bh,
									   grp_target_blk,
									   &num,
									   NULL );

		if( 0 <= grp_alloc_blk )
		{
			goto allocated;
		}
	}
	/* ------------------------------------------------------------------------ */
	/* as for now, i do not implement free block search							*/
	/* ------------------------------------------------------------------------ */
	else
	{
		*err = -ENOSPC;
		goto out;
	}

allocated:
	DBGPRINT( "<ME2FS>%s: using block group = %lu, free blocks = %d \n",
			  __func__, group_no, gdesc->bg_free_blocks_count );
	
	ret_block = grp_alloc_blk + ext2GetFirstBlockNum( sb, group_no );

	if( IN_RANGE( le32_to_cpu( gdesc->bg_block_bitmap ), ret_block, num ) ||
		IN_RANGE( le32_to_cpu( gdesc->bg_inode_bitmap ), ret_block, num ) ||
		IN_RANGE( ret_block,
				  le32_to_cpu( gdesc->bg_inode_table ),
				  ME2FS_SB( sb )->s_itb_per_group ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:allocating block in system zone -", __func__ );
		ME2FS_ERROR( " block from %lu, length %lu\n", ret_block, num );
		/* -------------------------------------------------------------------- */
		/* as for now, i do not implement retry_alloc							*/
		/* -------------------------------------------------------------------- */
		*err = -ENOSPC;
		goto out;
	}

	performed_allocation = 1;

	if( le32_to_cpu( esb->s_blocks_count ) <= ( ret_block + num - 1 ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:blocks count(%d) <= block(%lu)",
					 __func__, le32_to_cpu( esb->s_blocks_count ), ret_block);
		ME2FS_ERROR( " - block group = %lu \n", group_no );
		goto out;
	}

	adjustGroupBlocks( sb, group_no, gdesc, gdesc_bh, -num );
	percpu_counter_sub( &msi->s_freeblocks_counter, num );

	mark_buffer_dirty( bitmap_bh );

	if( sb->s_flags & MS_SYNCHRONOUS )
	{
		sync_dirty_buffer( bitmap_bh );
	}

	*err = 0;

	brelse( bitmap_bh );

	if( num < *count )
	{
#if 0	// quota
		dquot_free_block_nodirty( inode, *count - num );
#endif
		mark_inode_dirty( inode );
		*count = num;
	}
	
	return( ret_block );

io_error:
	*err = -EIO;
out:
	if( !performed_allocation )
	{
#if 0	// quota
		dquot_free_block_nodirty( inode, *count );
#endif
		mark_inode_dirty( inode );
	}
	brelse( bitmap_bh );

	return( 0 );

}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsFreeblocks
	Input		:struct inode *inode
				 < vfs inode to free blocks >
				 unsigned long block_num
				 < block number to start to free blocks >
				 unsigned long count
				 < number of blocks to free >
	Output		:void
	Return		:void

	Description	:free blocks
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsFreeBlocks( struct inode *inode,
					  unsigned long block_num,
					  unsigned long count )
{
	struct buffer_head		*bitmap_bh;
	struct buffer_head		*gdesc_bh;
	unsigned long			block_group;
	unsigned long			bit;
	unsigned long			i;
	unsigned long			overflow;
	struct super_block		*sb;
	struct me2fs_sb_info	*msi;
	struct ext2_group_desc	*gdesc;
	struct ext2_super_block	*esb;
	unsigned				freed;
	unsigned				group_freed;

	sb			= inode->i_sb;
	esb			= ME2FS_SB( sb )->s_esb;
	bitmap_bh	= NULL;
	freed		= 0;
	
	if( block_num < le32_to_cpu( esb->s_first_data_block )	||
		( block_num + count ) < block_num					||
		le32_to_cpu( esb->s_blocks_count) < ( block_num + count ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:error : freeing blocks not in datazone\n",
					 __func__ );
		ME2FS_ERROR( "<ME2FS>block = %lu, count %lu\n", block_num, count );
		goto error_return;
	}

	msi			= ME2FS_SB( sb );

do_more:
	overflow	= 0;
	block_group	= ( block_num - le32_to_cpu( esb->s_first_data_block ) )
				  / msi->s_blocks_per_group;
	bit			= ( block_num - le32_to_cpu( esb->s_first_data_block ) )
				  % msi->s_blocks_per_group;
	
	/* ------------------------------------------------------------------------ */
	/* check to see if we are freeing blocks across a group boundary			*/
	/* ------------------------------------------------------------------------ */
	if( msi->s_blocks_per_group < ( bit + count ) )
	{
		overflow	= bit + count - msi->s_blocks_per_group;
		count		= count - overflow;
	}

	if( likely( bitmap_bh ) )
	{
		brelse( bitmap_bh );
	}

	if( !( bitmap_bh = readBlockBitmap( sb, block_group ) ) )
	{
		goto error_return;
	}

	gdesc		= me2fsGetGroupDescriptor( sb, block_group );

	if( !gdesc )
	{
		goto error_return;
	}

	gdesc_bh	= me2fsGetGdescBufferCache( sb, block_group );

	if( IN_RANGE( le32_to_cpu( gdesc->bg_block_bitmap ), block_num, count )	||
		IN_RANGE( le32_to_cpu( gdesc->bg_inode_bitmap ), block_num, count )	||
		IN_RANGE( block_num,
				  le32_to_cpu( gdesc->bg_inode_table ),
				  msi->s_itb_per_group )								||
		IN_RANGE( block_num + count - 1,
				  le32_to_cpu( gdesc->bg_inode_table ),
				  msi->s_itb_per_group ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:error:Freeing blocks in system zones\n",
					 __func__ );
		ME2FS_ERROR( "<ME2FS>block = %lu, count = %lu\n", block_num, count );
		goto error_return;
	}

	for( i = 0 , group_freed = 0 ; i < count ; i++ )
	{
		if( !ext2_clear_bit_atomic( getSbBlockGroupLock( msi, block_group ),
									bit + i,
									bitmap_bh->b_data ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:warning:bit already clreaed for block %lu\n",
						 __func__, block_num );
		}
		else
		{
			group_freed++;
		}
	}

	mark_buffer_dirty( bitmap_bh );

	if( sb->s_flags & MS_SYNCHRONOUS )
	{
		sync_dirty_buffer( bitmap_bh );
	}

	adjustGroupBlocks( sb, block_group, gdesc, gdesc_bh, group_freed );

	freed += group_freed;

	if( overflow )
	{
		block_num	+= count;
		count		= overflow;
		goto do_more;
	}

error_return:
	brelse( bitmap_bh );
	if( freed )
	{
		percpu_counter_add( &msi->s_freeblocks_counter, freed );
#if 0	// quota
		dquot_free_block_nodirty( inode, freed );
#endif
		mark_inode_dirty( inode );
	}
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetBlocksUsedByGroupTable
	Input		:struct super_block *sb
				 < vfs super block >
				 int group
				 < goupr number >
	Output		:void
	Return		:void

	Description	:get number of blocks used by the group table in group
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
unsigned long
me2fsGetBlocksUsedByGroupTable( struct super_block *sb, int group )
{
	if( me2fsHasBgSuper( sb, group ) )
	{
		return( ME2FS_SB( sb )->s_gdb_count );
	}
	
	return( 0 );
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
	Function	:isGroupSparse
	Input		:int group
				 < group number >
	Output		:void
	Return		:int
				 < result >

	Description	:is group sparsed?
==================================================================================
*/
static int isGroupSparse( int group )
{
	if( group <= 1 )
	{
		return( 1 );
	}

	return( testRoot( group, 3 ) ||
			testRoot( group, 5 ) ||
			testRoot( group, 7 ) );
}

/*
==================================================================================
	Function	:testRoot
	Input		:int group
				 < group number >
				 int multiple
				 < multiple number >
	Output		:void
	Return		:void

	Description	:test whether given group number is multiples of given
				 multiple number or not
==================================================================================
*/
static int testRoot( int group, int multiple )
{
	int		num;

	num = multiple;

	while( num < group )
	{
		num *= multiple;
	}

	return( num == group );
}

/*
==================================================================================
	Function	:hasFreeBlocks
	Input		:struct me2fs_sb_info *msi
				 < me2fs super block information >
	Output		:void
	Return		:int
				 < result >

	Description	:test whethrer filesystem has free blocks or not
==================================================================================
*/
static int hasFreeBlocks( struct me2fs_sb_info *msi )
{
	unsigned long	free_blocks;
	unsigned long	root_blocks;

	free_blocks = percpu_counter_read_positive( &msi->s_freeblocks_counter );
	root_blocks = le32_to_cpu( msi->s_esb->s_r_blocks_count );

	if( ( free_blocks < ( root_blocks + 1 ) )
		&& !capable( CAP_SYS_RESOURCE )
		&& !uid_eq( msi->s_resuid, current_fsuid( ) )
		&& ( gid_eq( msi->s_resgid, GLOBAL_ROOT_GID )
		   || !in_group_p( msi->s_resgid ) ) )
	{
		return( 0 );
	}

	return( 1 );
}

/*
==================================================================================
	Function	:readBlockBitmap
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned long block_group
				 < block group number to read bitmap >
	Output		:void
	Return		:struct buffer_head*
				 < buffer cache bimap belongs to >

	Description	:read bitmap for a given block group
==================================================================================
*/
static struct buffer_head*
readBlockBitmap( struct super_block *sb, unsigned long block_group )
{
	struct ext2_group_desc	*gdesc;
	struct buffer_head		*bh;
	unsigned long			bitmap_blk;

	if( !( gdesc = me2fsGetGroupDescriptor( sb, block_group ) ) )
	{
		return( NULL );
	}

	bitmap_blk	= le32_to_cpu( gdesc->bg_block_bitmap );
	bh			= sb_getblk( sb, bitmap_blk );

	if( unlikely( !bh ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:cannot read block bitmap", __func__ );
		ME2FS_ERROR( "block_group=%lu, block_bitmap=%u\n",
					 block_group, le32_to_cpu( gdesc->bg_block_bitmap ) );
		return( NULL );
	}

	if( likely( bh_uptodate_or_lock( bh ) ) )
	{
		return( bh );
	}

	if( bh_submit_read( bh ) < 0 )
	{
		brelse( bh );
		ME2FS_ERROR( "<ME2FS>%s:cannot read block bitmap", __func__ );
		ME2FS_ERROR( "block_group=%lu, block_bitmap=%u\n",
					 block_group, le32_to_cpu( gdesc->bg_block_bitmap ) );
		return( NULL );
	}

	/* ------------------------------------------------------------------------ */
	/* sanity check for bitmap.(here this is just for displaying error)			*/
	/* ------------------------------------------------------------------------ */
	validBlockBitmap( sb, gdesc, block_group, bh );

	return( bh );

}
/*
==================================================================================
	Function	:validBlockBitmap
	Input		:struct super_block *sb
				 < vfs super block >
				 struct ext2_group_desc *gdesc
				 < group descriptor to validate >
				 unsigned long block_group
				 < block group number to validate >
				 struct buffer_head *bh
				 < buffer chaceh bitmap belongs to >
	Output		:void
	Return		:int
				 < validation result >

	Description	:validate block bitmap
==================================================================================
*/
static int
validBlockBitmap( struct super_block *sb,
				  struct ext2_group_desc *gdesc,
				  unsigned long block_group,
				  struct buffer_head *bh )
{
	unsigned long	offset;
	unsigned long	next_zero_bit;
	unsigned long	bitmap_blk;
	unsigned long	group_first_block;

	group_first_block = ext2GetFirstBlockNum( sb, block_group );

	/* ------------------------------------------------------------------------ */
	/* check whether block bitmap block number is set							*/
	/* ------------------------------------------------------------------------ */
	bitmap_blk	= le32_to_cpu( gdesc->bg_block_bitmap );
	offset		= bitmap_blk - group_first_block;
	if( !test_bit_le( offset, bh->b_data ) )
	{
		/* bad block bitmap														*/
		goto err_out;
	}

	/* ------------------------------------------------------------------------ */
	/* check whether inode bitmap block number is set							*/
	/* ------------------------------------------------------------------------ */
	bitmap_blk	= le32_to_cpu( gdesc->bg_inode_bitmap );
	offset		= bitmap_blk - group_first_block;
	if( !test_bit_le( offset, bh->b_data ) )
	{
		/* bad block bitmap														*/
		goto err_out;
	}

	/* ------------------------------------------------------------------------ */
	/* check whether inode table block number is set							*/
	/* ------------------------------------------------------------------------ */
	bitmap_blk		= le32_to_cpu( gdesc->bg_inode_table );
	offset			= bitmap_blk - group_first_block;
	next_zero_bit	= find_next_zero_bit_le( bh->b_data,
											 offset
											 + ME2FS_SB( sb )->s_itb_per_group,
											 offset );
	
	if( ( offset + ME2FS_SB( sb )->s_itb_per_group ) <= next_zero_bit )
	{
		/* good bitmap for inode tables											*/
		return( 1 );
	}

err_out:
	ME2FS_ERROR( "<ME2FS>%s:Invalid block bitmap -", __func__ );
	ME2FS_ERROR( "block_group = %lu, block=%lu\n", block_group, bitmap_blk );
	return( 0 );
}

/*
==================================================================================
	Function	:tryToAllocate
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned long group
				 < group number >
				 struct buffer_head *bitmap_bh
				 < buffer cache block bitmap belongs to >
				 unsigned long grp_goal
				 < block number of goal within the group >
				 unsigned long *count
				 < number of blocks to allocate >
				 struct ext2_reserve_window *my_rsv
				 < reservation window >
	Output		:void
	Return		:void

	Description	:attempt to allocate blocks within a given range
==================================================================================
*/
static int
tryToAllocate( struct super_block *sb,
			   unsigned long group,
			   struct buffer_head *bitmap_bh,
			   unsigned long grp_goal,
			   unsigned long *count,
			   struct ext2_reserve_window *my_rsv )
{
	//unsigned long	group_first_block;
	unsigned long	start;
	unsigned long	end;
	unsigned long	num;

	num		= 0;

	end		= ME2FS_SB( sb )->s_blocks_per_group;

repeat:
	start	= grp_goal;

	if( ext2_set_bit_atomic( getSbBlockGroupLock( ME2FS_SB( sb ), group ),
							 grp_goal,
							 bitmap_bh->b_data ) )
	{
		/* -------------------------------------------------------------------- */
		/* the block was already allocated by another thread, or it was			*/
		/* allocated and then freed by another thread							*/
		/* -------------------------------------------------------------------- */
		start++;
		grp_goal++;
		if( end <= start )
		{
			goto fail_access;
		}

		goto repeat;
	}

	num++;

	*count = num;
	return( grp_goal );

fail_access:
	*count = num;
	return( -1 );
}
/*
==================================================================================
	Function	:adjustGroupBlocks
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned long group_no
				 < number of group to adjust >
				 struct ext2_group_desc *desc
				 < group descriptor to adjust >
				 struct buffer_head *bh
				 < buffer head group descriptor belongs to >
				 long count
				 < adjust number >
	Output		:void
	Return		:void

	Description	:adjust number of free blocks
==================================================================================
*/
static void
adjustGroupBlocks( struct super_block *sb,
				   unsigned long group_no,
				   struct ext2_group_desc *gdesc,
				   struct buffer_head *bh,
				   long count )
{
	if( count )
	{
		struct me2fs_sb_info	*msi;
		unsigned				free_blocks;

		msi = ME2FS_SB( sb );

		spin_lock( getSbBlockGroupLock( msi, group_no ) );

		free_blocks					= le16_to_cpu( gdesc->bg_free_blocks_count );
		gdesc->bg_free_blocks_count	= cpu_to_le16( free_blocks + count );

		spin_unlock( getSbBlockGroupLock( msi, group_no ) );
		mark_buffer_dirty( bh );
	}
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
