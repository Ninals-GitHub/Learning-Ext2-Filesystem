/********************************************************************************
	File			: me2fs_inode.c
	Description		: inode operations for my ext2 file system

*********************************************************************************/
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_block.h"
#include "me2fs_inode.h"
#include "me2fs_dir.h"
#include "me2fs_namei.h"

/*
==================================================================================

	DEFINES

==================================================================================
*/
typedef struct
{
	__le32				*p;
	__le32				key;
	struct buffer_head	*bh;
} Indirect;


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
/*
----------------------------------------------------------------------------------

	Address Space Operations for Ext2 Inode

----------------------------------------------------------------------------------
*/
static int me2fsReadPage( struct file *filp, struct page *page );
static int me2fsReadPages( struct file *filp,
						   struct address_space *mapping,
						   struct list_head *pages,
						   unsigned nr_pages );

/*
----------------------------------------------------------------------------------

	Helper Functions for Inode

----------------------------------------------------------------------------------
*/
static struct ext2_inode*
me2fsGetExt2Inode( struct super_block *sb,
				   unsigned long ino,
				   struct buffer_head **bhp );
static int me2fsGetBlocks( struct inode *inode,
						   sector_t iblock,
						   unsigned long maxblocks,
						   struct buffer_head *bh_result,
						   int create );
static int
me2fsBlockToPath( struct inode *inode,
				  unsigned long i_block,
				  int offsets[ 4 ],
				  int *boundary );
static Indirect*
me2fsGetBranch( struct inode	*inode,
				int				depth,
				int				*offsets,
				Indirect		chain[ 4 ],
				int				*err );
static inline void addChain( Indirect *p, struct buffer_head *bh, __le32 *v );

/*
==================================================================================

	Management

==================================================================================
*/
//static int me2fsReadPage( struct file *file, struct page *page )
//{ DBGPRINT( "<ME2FS>AOPS:readpage!\n" ); return 0; }
//static int me2fsReadPages( struct file *filp, struct address_space *mapping,
//						   struct list_head *pages, unsigned nr_pages )
//{ DBGPRINT( "<ME2FS>AOPS:readpages 2!\n" ); return 0; }
static int me2fsWritePage( struct page *page, struct writeback_control *wbc )
{ DBGPRINT( "<ME2FS>AOPS:writepage!\n" ); return 0; }
static int me2fsWriteBegin( struct file *file, struct address_space *mapping,
							loff_t pos, unsigned len, unsigned flags,
							struct page **pagep, void **fsdata )
{ DBGPRINT( "<ME2FS>AOPS:write_begin!\n" ); return 0; }
static int me2fsWriteEnd( struct file *file, struct address_space *mapping,
						loff_t pos, unsigned len,
						unsigned copied, struct page *page, void *fsdata )
{ DBGPRINT( "<ME2FS>AOPS:write_end!\n" ); return 0; }
static sector_t me2fsBmap( struct address_space *mapping, sector_t sec )
{ DBGPRINT( "<ME2FS>AOPS:bmap!\n" ); return 0; }
static int me2fsWritePages( struct address_space *mapping,
							struct writeback_control *wbc )
{ DBGPRINT( "<ME2FS>AOPS:writepages 2!\n" ); return 0; }


/*
---------------------------------------------------------------------------------
	Address Space Operations
---------------------------------------------------------------------------------
*/
const struct address_space_operations me2fs_aops =
{
	.readpage			= me2fsReadPage,
	.readpages			= me2fsReadPages,
	.writepage			= me2fsWritePage,
	.write_begin		= me2fsWriteBegin,
	.write_end			= me2fsWriteEnd,
	.bmap				= me2fsBmap,
	//.direct_IO			= me2fsDirectIO,
	.writepages			= me2fsWritePages,
	.migratepage		= buffer_migrate_page,
	.is_partially_uptodate = block_is_partially_uptodate,
	.error_remove_page	= generic_error_remove_page,
};

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetVfsInode
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int ino
				 < inode number for allocating new inode >
	Output		:void
	Return		:struct inode *inode
				 < vfs inode >

	Description	:allocate me2fs inode info, get locked vfs inode, read ext2 inode
				 from a disk and fill them
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct inode *me2fsGetVfsInode( struct super_block *sb, unsigned int ino )
{
	struct inode			*inode;
	struct me2fs_inode_info	*mei;
	struct ext2_inode		*ext2_inode;
	struct buffer_head		*bh;
	uid_t					i_uid;
	gid_t					i_gid;
	int						i;

	/* ------------------------------------------------------------------------ */
	/* find an inode cache or allocate inode and lock it						*/
	/* ------------------------------------------------------------------------ */
	inode = iget_locked( sb, ino );

	if( !inode )
	{
		return( ERR_PTR( -ENOMEM ) );
	}

	/* ------------------------------------------------------------------------ */
	/* get an inode which has already existed									*/
	/* ------------------------------------------------------------------------ */
	if( !( inode->i_state & I_NEW ) )
	{
		return( inode );
	}
	
	/* ------------------------------------------------------------------------ */
	/* read ext2 inode															*/
	/* ------------------------------------------------------------------------ */
	mei = ME2FS_I( inode );

	ext2_inode = me2fsGetExt2Inode( inode->i_sb, ino, &bh );

	if( IS_ERR( ext2_inode ) )
	{
		iget_failed( inode );
		return( ( struct inode* )( ext2_inode ) );
	}

	dbgPrintExt2InodeInfo( ext2_inode );

	/* ------------------------------------------------------------------------ */
	/* set up vfs inode															*/
	/* ------------------------------------------------------------------------ */
	inode->i_mode	= le16_to_cpu( ext2_inode->i_mode );
	i_uid			= ( uid_t )le16_to_cpu( ext2_inode->i_uid );	// low bytes
	i_gid			= ( gid_t )le16_to_cpu( ext2_inode->i_gid );	// low bytes
	if( ME2FS_SB( sb )->s_mount_opt & EXT2_MOUNT_NO_UID32 )
	{
		/* do nothing															*/
	}
	else
	{
		i_uid |= le16_to_cpu( ext2_inode->osd2.linux2.l_i_uid_high ) << 16;
		i_gid |= le16_to_cpu( ext2_inode->osd2.linux2.l_i_gid_high ) << 16;
	}
	i_uid_write( inode, i_uid );
	i_gid_write( inode, i_gid );
	set_nlink( inode, le16_to_cpu( ext2_inode->i_links_count ) );
	inode->i_size	= le32_to_cpu( ext2_inode->i_size );
	inode->i_atime.tv_sec	= ( signed int )le32_to_cpu( ext2_inode->i_atime );
	inode->i_ctime.tv_sec	= ( signed int )le32_to_cpu( ext2_inode->i_ctime );
	inode->i_mtime.tv_sec	= ( signed int )le32_to_cpu( ext2_inode->i_mtime );
	inode->i_atime.tv_nsec	= 0;
	inode->i_ctime.tv_nsec	= 0;
	inode->i_mtime.tv_nsec	= 0;
	mei->i_dtime			= le32_to_cpu( ext2_inode->i_dtime );

	if( ( inode->i_nlink == 0 ) &&
		( ( inode->i_mode == 0 ) || ( mei->i_dtime ) ) )
	{
		brelse( bh );
		iget_failed( inode );
		return( ERR_PTR( -ESTALE ) );
	}

	inode->i_blocks			= le32_to_cpu( ext2_inode->i_blocks );
	mei->i_flags			= le32_to_cpu( ext2_inode->i_flags );
	mei->i_faddr			= le32_to_cpu( ext2_inode->i_faddr );
	mei->i_frag_no			= ext2_inode->osd2.linux2.l_i_frag;
	mei->i_frag_size		= ext2_inode->osd2.linux2.l_i_fsize;
	mei->i_file_acl			= le32_to_cpu( ext2_inode->i_file_acl );
	mei->i_dir_acl			= 0;
	if( S_ISREG( inode->i_mode ) )
	{
		/* use i_dir_acl higher 32 byte of file size							*/
		inode->i_size		|= ( ( __u64 )le32_to_cpu( ext2_inode->i_dir_acl ) )
							   << 32;
	}
	else
	{
		mei->i_dir_acl		= le32_to_cpu( ext2_inode->i_dir_acl );
	}
	mei->i_dtime			= 0;
	inode->i_generation		= le32_to_cpu( ext2_inode->i_generation );
	mei->i_state			= 0;
	mei->i_block_group		= ( ino - 1 ) / ME2FS_SB( sb )->s_inodes_per_group;
	mei->i_dir_start_lookup	= 0;

	for( i = 0 ; i < ME2FS_NR_BLOCKS ; i++ )
	{
		mei->i_data[ i ] = ext2_inode->i_block[ i ];
	}

	if( S_ISREG( inode->i_mode ) )
	{
	}
	else if( S_ISDIR( inode->i_mode ) )
	{
		DBGPRINT( "<ME2FS>get directory inode!\n" );
		inode->i_fop = &me2fs_dir_operations;
		inode->i_op = &me2fs_dir_inode_operations;
		inode->i_mapping->a_ops = &me2fs_aops;
	}
	else if( S_ISLNK( inode->i_mode ) )
	{
	}
	else
	{
	}

	brelse( bh );
	me2fsSetVfsInodeFlags( inode );

	dbgPrintMe2fsInodeInfo( mei );
	dbgPrintVfsInode( inode );

	unlock_new_inode( inode );

	return( inode );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetVfsInodeFlags
	Input		:struct inode *inode
				 < target vfs inode >
	Output		:struct inode *inode
				 < written to inode->i_flags >
	Return		:void

	Description	:write ext2 inode flags to vfs inode flags
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsSetVfsInodeFlags( struct inode *inode )
{
	unsigned int	flags;

	flags = ME2FS_I( inode )->i_flags;

	inode->i_flags &= ~( S_SYNC | S_APPEND | S_IMMUTABLE | S_NOATIME | S_DIRSYNC );

	if( flags & EXT2_SYNC_FL )
	{
		inode->i_flags |= S_SYNC;
	}

	if( flags & EXT2_APPEND_FL )
	{
		inode->i_flags |= S_APPEND;
	}

	if( flags & EXT2_IMMUTABLE_FL )
	{
		inode->i_flags |= S_IMMUTABLE;
	}

	if( flags & EXT2_NOATIME_FL )
	{
		inode->i_flags |= S_NOATIME;
	}

	if( flags & EXT2_DIRSYNC_FL )
	{
		inode->i_flags |= S_DIRSYNC;
	}
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetMe2fsInodeFlags
	Input		:struct me2fs_inode_info *mei
				 < me2fs inode information >
	Output		:struct me2fs_inode_info *mei
				 < written to i_flags >
	Return		:void

	Description	:write vfs inode flags to ext2 inode flags
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsSetMe2fsInodeFlags( struct me2fs_inode_info *mei )
{
	unsigned int	flags;

	flags = mei->vfs_inode.i_flags;

	mei->i_flags &= ~( EXT2_SYNC_FL | EXT2_APPEND_FL | EXT2_IMMUTABLE_FL |
					   EXT2_NOATIME_FL | EXT2_DIRSYNC_FL );
	
	if( flags & S_SYNC )
	{
		mei->i_flags |= EXT2_SYNC_FL;
	}
	if( flags & S_APPEND )
	{
		mei->i_flags |= EXT2_APPEND_FL;
	}
	if( flags & S_IMMUTABLE )
	{
		mei->i_flags |= EXT2_IMMUTABLE_FL;
	}
	if( flags & S_NOATIME )
	{
		mei->i_flags |= EXT2_NOATIME_FL;
	}
	if( flags & S_DIRSYNC )
	{
		mei->i_flags |= EXT2_DIRSYNC_FL;
	}
}

/*
----------------------------------------------------------------------------------

	Helper Functions

----------------------------------------------------------------------------------
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:dbgPrintVfsInode
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:print vfs inode debug information
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void dbgPrintVfsInode( struct inode *inode )
{
	
	DBGPRINT( "<ME2FS>[vfs inode information]\n" );
	DBGPRINT( "<ME2FS>i_mode = %4X\n", inode->i_mode );
	DBGPRINT( "<ME2FS>i_opflags = %u\n", inode->i_opflags );
	DBGPRINT( "<ME2FS>i_uid = %u\n", __kuid_val( inode->i_uid ) );
	DBGPRINT( "<ME2FS>i_gid = %u\n", __kgid_val( inode->i_gid ) );
	DBGPRINT( "<ME2FS>i_flags = %u\n", inode->i_flags );
	DBGPRINT( "<ME2FS>i_nlink = %u\n", inode->i_nlink );
	DBGPRINT( "<ME2FS>i_rdev = %u\n", inode->i_rdev );
	DBGPRINT( "<ME2FS>i_size = %llu\n", inode->i_size );
	DBGPRINT( "<ME2FS>i_atime.tv_sec = %lu\n", inode->i_atime.tv_sec );
	DBGPRINT( "<ME2FS>i_atime.tv_nsec = %lu\n", inode->i_atime.tv_nsec );
	DBGPRINT( "<ME2FS>i_mtime.tv_sec = %lu\n", inode->i_mtime.tv_sec );
	DBGPRINT( "<ME2FS>i_mtime.tv_nsec = %lu\n", inode->i_mtime.tv_nsec );
	DBGPRINT( "<ME2FS>i_ctime.tv_sec = %lu\n", inode->i_ctime.tv_sec );
	DBGPRINT( "<ME2FS>i_ctime.tv_nsec = %lu\n", inode->i_ctime.tv_nsec );
	DBGPRINT( "<ME2FS>i_bytes = %u\n", inode->i_bytes );
	DBGPRINT( "<ME2FS>i_blkbits = %u\n", inode->i_blkbits );
	DBGPRINT( "<ME2FS>i_blocks = %lu\n", inode->i_blocks );
	DBGPRINT( "<ME2FS>i_state = %lu\n", inode->i_state );
	DBGPRINT( "<ME2FS>dirtied_when = %lu\n", inode->dirtied_when );
	DBGPRINT( "<ME2FS>i_version = %llu\n", inode->i_version );
	DBGPRINT( "<ME2FS>i_count = %d\n", atomic_read( &inode->i_count ) );
	DBGPRINT( "<ME2FS>i_dio_count = %d\n", atomic_read( &inode->i_dio_count ) );
	DBGPRINT( "<ME2FS>i_writecount = %d\n", atomic_read( &inode->i_writecount ) );
	DBGPRINT( "<ME2FS>i_generation = %u\n", inode->i_generation );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:dbgPrintMe2fsInodeInfo
	Input		:struct me2fs_inode_info *mei
				 < me2fs ext2 inode information >
	Output		:void
	Return		:void

	Description	:print me2fs inode information
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void dbgPrintMe2fsInodeInfo( struct me2fs_inode_info *mei )
{
	int				i;
	unsigned int	value;

	DBGPRINT( "<ME2FS>[me2fs inode information]\n" );
	DBGPRINT( "<ME2FS>i_data:\n" );
	for( i = 0 ; i < ME2FS_NR_BLOCKS ; i++ )
	{
		value = le32_to_cpu( mei->i_data[ i ] );
		DBGPRINT( "<ME2FS>i_data[%d] = %u\n", i, value );
	}
	DBGPRINT( "<ME2FS>i_flags = %u\n", mei->i_flags );
	DBGPRINT( "<ME2FS>i_faddr = %u\n", mei->i_faddr );
	DBGPRINT( "<ME2FS>i_frag_no = %u\n", ( unsigned int )mei->i_frag_no );
	DBGPRINT( "<ME2FS>i_frag_size = %u\n", ( unsigned int )mei->i_frag_size );
	DBGPRINT( "<ME2FS>i_state = %u\n", mei->i_state );
	DBGPRINT( "<ME2FS>i_file_acl = %u\n", mei->i_file_acl );
	DBGPRINT( "<ME2FS>i_dir_cl = %u\n", mei->i_dir_acl );
	DBGPRINT( "<ME2FS>i_dtime = %u\n", mei->i_dtime );
	DBGPRINT( "<ME2FS>i_block_group = %u\n", mei->i_block_group );
	DBGPRINT( "<ME2FS>i_dir_start_lookup = %u\n", mei->i_dir_start_lookup );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:dbgPrintExt2InodeInfo
	Input		:struct ext2_inode *ei
				 < ext2 inode >
	Output		:void
	Return		:void

	Description	:print ext2 inode debug information
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void dbgPrintExt2InodeInfo( struct ext2_inode *ei )
{
	int				i;
	unsigned int	value;
	DBGPRINT( "<ME2FS>[ext2 inode information]\n" );
	value = le16_to_cpu( ei->i_mode );
	DBGPRINT( "<ME2FS>i_mode = %4X\n", value );
	value = le16_to_cpu( ei->i_uid );
	DBGPRINT( "<ME2FS>i_uid = %u\n", value );
	value = le32_to_cpu( ei->i_size );
	DBGPRINT( "<ME2FS>i_size = %u\n", value );
	value = le32_to_cpu( ei->i_atime );
	DBGPRINT( "<ME2FS>i_atime = %u\n", value );
	value = le32_to_cpu( ei->i_ctime );
	DBGPRINT( "<ME2FS>i_ctime = %u\n", value );
	value = le32_to_cpu( ei->i_mtime );
	DBGPRINT( "<ME2FS>i_mtime = %u\n", value );
	value = le32_to_cpu( ei->i_dtime );
	DBGPRINT( "<ME2FS>i_dtime = %u\n", value );
	value = le16_to_cpu( ei->i_gid );
	DBGPRINT( "<ME2FS>i_gid = %u\n", value );
	value = le16_to_cpu( ei->i_links_count );
	DBGPRINT( "<ME2FS>i_links_count = %u\n", value );
	value = le32_to_cpu( ei->i_blocks );
	DBGPRINT( "<ME2FS>i_blocks = %u\n", value );
	value = le32_to_cpu( ei->i_flags );
	DBGPRINT( "<ME2FS>i_flags = %u\n", value );
	DBGPRINT( "<ME2FS>i_block:\n" );
	for( i = 0 ; i < ME2FS_NR_BLOCKS ; i++ )
	{
		value = le32_to_cpu( ei->i_block[ i ] );
		DBGPRINT( "<ME2FS>i_block[ %d ] = %u\n", i, value );
	}
	value = le32_to_cpu( ei->i_generation );
	DBGPRINT( "<ME2FS>i_generation = %u\n", value );
	value = le32_to_cpu( ei->i_file_acl );
	DBGPRINT( "<ME2FS>i_file_acl = %u\n", value );
	value = le32_to_cpu( ei->i_dir_acl );
	DBGPRINT( "<ME2FS>i_dir_acl = %u\n", value );
	value = le32_to_cpu( ei->i_faddr );
	DBGPRINT( "<ME2FS>i_faddr = %u\n", value );
	DBGPRINT( "<ME2FS>l_i_frag = %u\n",
			  ( unsigned int )ei->osd2.linux2.l_i_frag );
	DBGPRINT( "<ME2FS>l_i_fsize = %u\n",
			  ( unsigned int )ei->osd2.linux2.l_i_fsize );
	value = le16_to_cpu( ei->osd2.linux2.l_i_uid_high );
	DBGPRINT( "<ME2FS>l_i_uid_high = %u\n", value );
	value = le16_to_cpu( ei->osd2.linux2.l_i_gid_high );
	DBGPRINT( "<ME2FS>l_i_gid_high = %u\n", value );

}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetBlock
	Input		:struct inode *inode
				 < vfs inode >
				 sector_t iblock
				 < block number in file >
				 struct buffer_head *bh_result
				 < buffer cache for the block >
				 int create
				 < 0 : plain lookup, 1 : creation >
	Output		:struct buffer_head *bh_result
				 < buffer cache for the block >
	Return		:int
				 < result >

	Description	:get block in file or allocate block for file
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsGetBlock( struct inode *inode,
				   sector_t iblock,
				   struct buffer_head *bh_result,
				   int create )
{
	unsigned long	maxblocks;
	int				ret;

	if( !inode )
	{
		ME2FS_ERROR( "<ME2FS>%s:inode is null!\n", __func__ );
		return( 0 );
	}

	DBGPRINT( "<ME2FS>%s:inode info:\n", __func__ );
	DBGPRINT( "<ME2FS>ino=%lu\n", inode->i_ino );

	maxblocks = bh_result->b_size >> inode->i_blkbits;

	ret = me2fsGetBlocks( inode, iblock, maxblocks, bh_result, create );

	if( 0 < ret )
	{
		bh_result->b_size = ( ret << inode->i_blkbits );
		ret = 0;
	}

	return( ret );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Local Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
----------------------------------------------------------------------------------

	Address Space Operations for Ext2 Inode

----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:me2fsReadPage
	Input		:struct file *filp
				 < vfs file object >
				 struct page *page
				 < information of page to be filled with data >
	Output		:void
	Return		:int
				 < result >

	Description	: make read request and fill data to buffer cache
==================================================================================
*/
static int me2fsReadPage( struct file *filp, struct page *page )
{
	DBGPRINT( "<ME2FS>read page\n" );
	return( mpage_readpage( page, me2fsGetBlock ) );
}

/*
==================================================================================
	Function	:me2fsReadPages
	Input		:struct file *filp
				 < vfs file object >
				 struct address_space *mapping >
				 < address space associated with the file >
				 struct list_head *pages
				 < information of pages to be filled with read data >
				 unsigned nr_pages
				 < number of pages >
	Output		:void
	Return		:void

	Description	:make multiple read requests and fill data to buffer cache
==================================================================================
*/
static int me2fsReadPages( struct file *filp,
						   struct address_space *mapping,
						   struct list_head *pages,
						   unsigned nr_pages )
{
	DBGPRINT( "<ME2FS>read page[s]\n" );
	return( mpage_readpages( mapping, pages, nr_pages, me2fsGetBlock ) );
}
/*
----------------------------------------------------------------------------------

	Helper Functions for Inode

----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:me2fsGetExt2Inode
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int ino
				 < inode number to get >
				 struct buffer_head **bhp
				 < buffer head pointer >
	Output		:struct buffer_head **bhp
				 < buffer cache to be read inode >
	Return		:struct ext2_inode
				 < inode of ext2 >

	Description	:read a ext2 inode from a disk and put it to buffer cache
==================================================================================
*/
static struct ext2_inode*
me2fsGetExt2Inode( struct super_block *sb,
				   unsigned long ino,
				   struct buffer_head **bhp )
{
	struct ext2_group_desc	*gdesc;
	unsigned long			block_group;
	unsigned long			block_offset;		// offset in a block group
	unsigned long			inode_index;
	unsigned long			inode_block;

	*bhp = NULL;

	/* ------------------------------------------------------------------------ */
	/* sanity check for inode number											*/
	/* ------------------------------------------------------------------------ */
	if( ( ino != ME2FS_EXT2_ROOT_INO ) &&
		( ino < ME2FS_SB( sb )->s_first_ino ) )
	{
		ME2FS_ERROR( "<ME2FS>failed to get ext2 inode [1] (ino=%lu)\n", ino );
		return( ERR_PTR( -EINVAL ) );
	}

	if( le32_to_cpu( ME2FS_SB( sb )->s_esb->s_inodes_count ) < ino )
	{
		ME2FS_ERROR( "<ME2FS>failed to get ext2 inode [2] (ino=%lu)\n", ino );
		return( ERR_PTR( -EINVAL ) );
	}

	/* ------------------------------------------------------------------------ */
	/* get group descriptor														*/
	/* ------------------------------------------------------------------------ */
	block_group = ( ino - 1 ) / ME2FS_SB( sb )->s_inodes_per_group;
	gdesc = ( struct ext2_group_desc* )me2fsGetGroupDescriptor( sb, block_group );
	if( !gdesc )
	{
		ME2FS_ERROR( "<ME2FS>cannot find group descriptor (ino=%lu)\n", ino );

		return( ERR_PTR( -EIO ) );
	}
	
	/* ------------------------------------------------------------------------ */
	/* get inode block number and read it										*/
	/* ------------------------------------------------------------------------ */
	block_offset	= ( ( ino - 1 ) % ME2FS_SB( sb )->s_inodes_per_group )
					  * ME2FS_SB( sb )->s_inode_size;
	inode_index		= block_offset >> sb->s_blocksize_bits;
	inode_block		= le32_to_cpu( gdesc->bg_inode_table ) + inode_index;
	
	if( !( *bhp = sb_bread( sb, inode_block ) ) )
	{
		ME2FS_ERROR( "<ME2FS>unable to read inode block [1].\n" );
		ME2FS_ERROR( "<ME2FS>( ino=%lu )\n", ino );

		return( ERR_PTR( -EIO ) );
	}
	
	block_offset &= ( sb->s_blocksize - 1 );

	return( ( struct ext2_inode* )( ( *bhp )->b_data + block_offset ) );
}


/*
==================================================================================
	Function	:me2fsGetBlocks
	Input		:struct inode *inode
				 < vfs inode >
				 sector_t iblock
				 < block number in file >
				 unsigned long maxblocks
				 < max blocks to get >
				 struct buffer_head *bh_result
				 < buffer cache for the block >
				 int create
				 < 0 : plain lookup, 1 : creation >
	Output		:struct buffer_head *bh_result
				 < buffer cache for the block >
	Return		:int
				 < number of blocks >

	Description	:get blocks in file or allocate blocks for file
==================================================================================
*/
static int me2fsGetBlocks( struct inode *inode,
						   sector_t iblock,
						   unsigned long maxblocks,
						   struct buffer_head *bh_result,
						   int create )
{
	Indirect			chain[ 4 ];
	Indirect			*partial;
	int					offsets[ 4 ];
	int					blocks_to_boundary;
	int					count;
	int					depth;
	int					err;

	/* ------------------------------------------------------------------------ */
	/* translate block number to its reference path								*/
	/* ------------------------------------------------------------------------ */
	if( !( depth = me2fsBlockToPath( inode,
									 iblock,
									 offsets,
									 &blocks_to_boundary  ) ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:cannot translate block number \n", __func__ );
		ME2FS_ERROR( "<ME2FS>iblock=%lu\n", iblock );
		return( -EIO );
	}

	/* ------------------------------------------------------------------------ */
	/* find a block																*/
	/* ------------------------------------------------------------------------ */
	count = 0;
	partial = me2fsGetBranch( inode, depth, offsets, chain, &err );

	if( !partial )
	{
		unsigned long	first_block;

		first_block = le32_to_cpu( chain[ depth - 1 ].key );
		clear_buffer_new( bh_result );
		count++;
		/* -------------------------------------------------------------------- */
		/* find more available blocks											*/
		/* -------------------------------------------------------------------- */
		while( ( count < maxblocks ) && ( count <= blocks_to_boundary ) )
		{
			unsigned long	cur_blk;

			/* ---------------------------------------------------------------- */
			/* as for now, i don't implement the verify function for truncate	*/
			/* ---------------------------------------------------------------- */
			/* do nothing */


			cur_blk = le32_to_cpu( *( chain[ depth - 1 ].p + count ) );

			if( cur_blk == first_block + count )
			{
				count++;
			}
			else
			{
				break;
			}
		}

		if( err != -EAGAIN )
		{
			goto found;
		}
	}

	if( !create || ( err == -EIO ) )
	{
		goto cleanup;
	}

	/* ------------------------------------------------------------------------ */
	/* as for now, i don't implement allocation									*/
	/* ------------------------------------------------------------------------ */
	/* do nothing */
	goto cleanup;

found:
	map_bh( bh_result, inode->i_sb, le32_to_cpu( chain[ depth - 1 ].key ) );
	/* i dont't care about boundary */
	err = count;

cleanup:
	DBGPRINT( "<ME2FS>%s:cleanup\n", __func__ );
	while( chain < partial )
	{
		brelse( partial->bh );
		partial--;
	}
	return( err );
}

/*
==================================================================================
	Function	:me2fsBlockToPath
	Input		:struct inode *inode
				 < vfs inode >
				 unsigned long i_block
				 < block number to translate to path >
				 int *offsets
				 < offset array for indirects >
				 int *boundary
	Output		:void
	Return		:int *offsets
				 < get offsets >
				 int *boundary
				 < boundary block number >

	Description	:translate block number to path to i_block
==================================================================================
*/
static int
me2fsBlockToPath( struct inode *inode,
				  unsigned long i_block,
				  int offsets[ 4 ],
				  int *boundary )
{
	unsigned int	ind_blk_entries;		/* number of entries per block		*/
	unsigned long	direct_blocks;			/* number of direct blocks			*/
	unsigned long	first_ind_blocks;		/* number of 1st indirect blocks	*/
	unsigned long	second_ind_blocks;		/* number of 2nd indirect blocks	*/
	unsigned long	third_ind_blocks;		/* number of 34d indirect blocks	*/
	unsigned long	final;
	int				depth;

	ind_blk_entries		= inode->i_sb->s_blocksize / sizeof( __le32 );
	direct_blocks		= ME2FS_NDIR_BLOCKS;

	final				= 0;
	depth				= 0;
	//ib_info->block_num	= i_block;

	/* ------------------------------------------------------------------------ */
	/* direct block																*/
	/* ------------------------------------------------------------------------ */
	if( i_block < direct_blocks )
	{
		offsets[ depth++ ]	= i_block;
		final				= direct_blocks;
		DBGPRINT( "<ME2FS>get blocks:\n" );
		DBGPRINT( "<ME2FS>i_block=%lu, depth=%d\n", i_block, depth );
		goto calc_boundary;
	}

	/* ------------------------------------------------------------------------ */
	/* 1st indirect blocks														*/
	/* ------------------------------------------------------------------------ */
	first_ind_blocks	= direct_blocks + ind_blk_entries;

	if( i_block < first_ind_blocks )
	{
		i_block				= i_block - direct_blocks;

		offsets[ depth++ ]	= ME2FS_IND_BLOCK;
		offsets[ depth++ ]	= i_block;

		final			= ind_blk_entries;

		DBGPRINT( "<ME2FS>get blocks:\n" );
		DBGPRINT( "<ME2FS>i_block=%lu, depth=%d\n", i_block, depth );
		goto calc_boundary;
	}

	/* ------------------------------------------------------------------------ */
	/* 2nd indirect blocks														*/
	/* ------------------------------------------------------------------------ */
	second_ind_blocks	= first_ind_blocks + ind_blk_entries * ind_blk_entries;

	if( i_block < second_ind_blocks )
	{
		i_block	= i_block - first_ind_blocks;

		offsets[ depth++ ]	= ME2FS_2IND_BLOCK;
		offsets[ depth++ ]	= i_block / ind_blk_entries;
		offsets[ depth++ ]	= i_block
							  & ( ind_blk_entries - 1 );

		final			= ind_blk_entries;

		DBGPRINT( "<ME2FS>get blocks:\n" );
		DBGPRINT( "<ME2FS>i_block=%lu, depth=%d\n", i_block, depth );
		goto calc_boundary;
	}

	/* ------------------------------------------------------------------------ */
	/* 3rd indirect blocks														*/
	/* ------------------------------------------------------------------------ */
	third_ind_blocks	= second_ind_blocks
					  + ( ind_blk_entries * ind_blk_entries * ind_blk_entries );
	
	if( i_block < third_ind_blocks )
	{
		unsigned long	i_index;
		i_block	= i_block - second_ind_blocks;
		i_index	= ( i_block / ( ind_blk_entries * ind_blk_entries ) )
				  & ( ind_blk_entries - 1 );
		offsets[ depth++ ]	= ME2FS_3IND_BLOCK;
		offsets[ depth++ ]	= i_index;
		offsets[ depth++ ]	= ( i_block / ind_blk_entries )
							  & ( ind_blk_entries - 1 );
		offsets[ depth++ ]	= i_block
							  & ( ind_blk_entries - 1 );
		final			= ind_blk_entries;
		DBGPRINT( "<ME2FS>get blocks:\n" );
		DBGPRINT( "<ME2FS>i_block=%lu, depth=%d\n", i_block, depth );
	}
	/* ------------------------------------------------------------------------ */
	/* too big block number														*/
	/* ------------------------------------------------------------------------ */
	else
	{
		ME2FS_ERROR( "<ME2FS>%s:Too big block number!!!!!", __func__ );
		final			= 0;
	}

	/* ------------------------------------------------------------------------ */
	/* get last block number													*/
	/* ------------------------------------------------------------------------ */
calc_boundary:
	if( boundary )
	{
		*boundary	= final - 1 - ( i_block & ( ind_blk_entries - 1 ) );
	}

	return( depth );
}

/*
==================================================================================
	Function	:me2fsGetBranch
	Input		:struct inode *inode
				 < vfs inode >
				 int depth
				 < depth of indirect >
				 int *offsets
				 < offsets array for indirects >
				 Indirect chain[ 4 ]
				 < indirect chain information >
				 int *err
				 < error information >
	Output		:Indirect*
				 < partial indirect >
	Return		:Indirect chain[ 4 ]
				 < update chain information >
				 int *err
				 < result >

	Description	:read direct and indirects block
==================================================================================
*/
static Indirect*
me2fsGetBranch( struct inode	*inode,
				int				depth,
				int				*offsets,
				Indirect		chain[ 4 ],
				int				*err )
{
	struct me2fs_inode_info	*mei;
	struct buffer_head		*bh;
	Indirect				*p;

	mei		= ME2FS_I( inode );
	p		= chain;
	*err	= 0;

	addChain( chain, NULL, ME2FS_I( inode )->i_data + *offsets );

	if( !p->key )
	{
		goto no_block;
	}

	while( --depth )
	{
		if( !( bh = sb_bread( inode->i_sb, le32_to_cpu( p->key ) ) ) )
		{
			*err = -EIO;
			goto no_block;
		}

		read_lock( &ME2FS_I( inode )->i_meta_lock );
		/* -------------------------------------------------------------------- */
		/* as for now, I do not implement detection change						*/
		/* -------------------------------------------------------------------- */
		addChain( ++p, bh, ( __le32* )bh->b_data + *( ++offsets ) );
		read_unlock( &ME2FS_I( inode )->i_meta_lock );

		if( !p->key )
		{
			goto no_block;
		}
	}

	return( NULL );

no_block:
	return( p );
}

/*
==================================================================================
	Function	:addChain
	Input		:Indirect *p
				 < indirect information >
				 struct buffer_head *bh
				 < buffer head indirect belongs to >
				 __le32 *v
				 < pointer to the key >
	Output		:void
	Return		:void

	Description	:add chain
==================================================================================
*/
static inline void addChain( Indirect *p, struct buffer_head *bh, __le32 *v )
{
	p->p	= v;
	p->key	= *v;
	p->bh	= bh;
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
