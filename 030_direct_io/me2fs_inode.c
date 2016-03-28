/********************************************************************************
	File			: me2fs_inode.c
	Description		: inode operations for my ext2 file system

*********************************************************************************/
#include <linux/buffer_head.h>
#include <linux/writeback.h>
#include <linux/mpage.h>
#include <linux/namei.h>
#include <linux/aio.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_block.h"
#include "me2fs_inode.h"
#include "me2fs_dir.h"
#include "me2fs_namei.h"
#include "me2fs_file.h"
#include "me2fs_symlink.h"
#include "me2fs_ialloc.h"
#include "me2fs_namei.h"
#include "me2fs_super.h"

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
static int me2fsWritePage( struct page *page, struct writeback_control *wbc );
static int me2fsWriteBegin( struct file *file,
							struct address_space *mapping,
							loff_t pos,
							unsigned len,
							unsigned flags,
							struct page **pagep,
							void **fsdata );
static int me2fsWriteEnd( struct file *file,
						  struct address_space *mapping,
						  loff_t pos,
						  unsigned len,
						  unsigned copied,
						  struct page *page,
						  void *fsdata );
static int me2fsWritePages( struct address_space *mapping,
							struct writeback_control *wbc );
static sector_t me2fsBmap( struct address_space *mapping, sector_t block );
static ssize_t
me2fsDirectIO( int rw,
			   struct kiocb *iocb,
			   struct iov_iter *iter,
			   loff_t offset );

/*
----------------------------------------------------------------------------------

	Helper Functions for Inode

----------------------------------------------------------------------------------
*/
static struct ext2_inode*
me2fsGetExt2Inode( struct super_block *sb,
				   unsigned long ino,
				   struct buffer_head **bhp );
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
static inline int
verifyIndirectChain( Indirect *from, Indirect *to );
static int me2fsGetBlocks( struct inode *inode,
						   sector_t iblock,
						   unsigned long maxblocks,
						   struct buffer_head *bh_result,
						   int create );
static inline void addChain( Indirect *p, struct buffer_head *bh, __le32 *v );
static int
__me2fsWriteInode( struct inode *inode, int do_sync );
static int allocBranch( struct inode *inode,
						int indirect_blks,
						int *blks,
						unsigned long goal,
						int *offsets,
						Indirect *branch );
static void spliceBranch( struct inode *inode,
						  long block,
						  Indirect *where,
						  int num,
						  int blks );
static unsigned long
findNear( struct inode *inode, Indirect *ind );
static inline unsigned long
findGoal( struct inode *inode, long block, Indirect *partial );
static inline int
isInodeFastSymlink( struct inode *inode );
static void truncateBlocks( struct inode *inode, loff_t offset );
static void __truncateBlocks( struct inode *inode, loff_t offset );
static inline void
freeData( struct inode *inode, __le32 *blocks, __le32 *end_block );
static Indirect*
findShared( struct inode *inode,
			int depth,
			int offsets[ 4 ],
			Indirect chain[ 4 ],
			__le32 *top );
static inline int
searchFirstNonZero( __le32 *cur, __le32 *end );
static void
freeBranches( struct inode *inode,
			  __le32 *cur,
			  __le32 *end,
			  int depth );
static int setInodeSize( struct inode *inode, loff_t newsize );
static void writeFailed( struct address_space *mapping, loff_t size );

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
#if 0
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
#endif
//static sector_t me2fsBmap( struct address_space *mapping, sector_t sec )
//{ DBGPRINT( "<ME2FS>AOPS:bmap!\n" ); return 0; }
#if 0
static int me2fsWritePages( struct address_space *mapping,
							struct writeback_control *wbc )
{ DBGPRINT( "<ME2FS>AOPS:writepages 2!\n" ); return 0; }
#endif


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
	.direct_IO			= me2fsDirectIO,
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
	DBGPRINT( "[1]vfs i_size = %lu\n", ( unsigned long )inode->i_size );
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
	DBGPRINT( "[2]vfs i_size = %lu\n", ( unsigned long )inode->i_size );
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
		inode->i_fop			= &me2fs_file_operations;
		inode->i_op				= &me2fs_file_inode_operations;
		inode->i_mapping->a_ops	= &me2fs_aops;
	}
	else if( S_ISDIR( inode->i_mode ) )
	{
		DBGPRINT( "<ME2FS>get directory inode!\n" );
		inode->i_fop			= &me2fs_dir_operations;
		inode->i_op				= &me2fs_dir_inode_operations;
		inode->i_mapping->a_ops	= &me2fs_aops;
	}
	else if( S_ISLNK( inode->i_mode ) )
	{
		if( isInodeFastSymlink( inode ) )
		{
			inode->i_op				= &me2fs_fast_symlink_inode_operations;
			nd_terminate_link( mei->i_data,
							   inode->i_size,
							   sizeof( mei->i_data ) - 1 );
		}
		else
		{
			inode->i_op				= &me2fs_symlink_inode_operations;
			inode->i_mapping->a_ops	= &me2fs_aops;
		}
	}
	else
	{
		inode->i_op = &me2fs_special_inode_operations;
		if( ext2_inode->i_block[ 0 ] )
		{
			init_special_inode( inode,
								inode->i_mode,
								old_decode_dev(
									le32_to_cpu( ext2_inode->i_block[ 0 ] )
								) );
		}
		else
		{
			init_special_inode( inode,
								inode->i_mode,
								new_decode_dev(
									le32_to_cpu( ext2_inode->i_block[ 1 ] )
								) );
		}
	}

	brelse( bh );
	me2fsSetVfsInodeFlags( inode );

	//dbgPrintMe2fsInodeInfo( mei );
	//dbgPrintVfsInode( inode );

	unlock_new_inode( inode );
	DBGPRINT( "[3]vfs i_size = %lu\n", ( unsigned long )inode->i_size );

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

	Superblock Operations

----------------------------------------------------------------------------------
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsWriteInode
	Input		:struct inode *inode
				 < vfs inode >
				 struct writeback_control *wbc
				 < write back information >
	Output		:void
	Return		:int
				 < result >

	Description	:commit writing an inode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsWriteInode( struct inode *inode, struct writeback_control *wbc )
{
	DBGPRINT( "<ME2FS>%s:ino=%lu\n", __func__, ( unsigned long )inode->i_ino );
	return( __me2fsWriteInode( inode, wbc->sync_mode == WB_SYNC_ALL ) );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsEvictInode
	Input		:struct inode *inode
	Output		:void
	Return		:void

	Description	:evict inode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsEvictInode( struct inode *inode )
{
	int		want_delete;

	if( !inode->i_nlink && !is_bad_inode( inode ) )
	{
		want_delete = 1;
	}
	else
	{
		want_delete = 0;
	}

	DBGPRINT( "<ME2FS>%s:info:evict inode [%lu]\n",
			  __func__, ( unsigned long )inode->i_ino );

	/* ------------------------------------------------------------------------ */
	/* truncate page cache														*/
	/* ------------------------------------------------------------------------ */
	truncate_inode_pages( &inode->i_data, 0 );

	//DBGPRINT( "<ME2FS>%s:info:end truncate_inode_pages\n", __func__ );
	//DBGPRINT( "<ME2FS>%s:info:want_delete(%d)\n", __func__, want_delete );

	if( want_delete )
	{
		/* -------------------------------------------------------------------- */
		/* wait until super block is unfreezed									*/
		/* -------------------------------------------------------------------- */
		sb_start_intwrite( inode->i_sb );

		ME2FS_I( inode )->i_dtime = get_seconds( );

		mark_inode_dirty( inode );

		/* if IS_SYNC or IS_DIRSYNC, then needs sync							*/
		__me2fsWriteInode( inode, inode_needs_sync( inode ) );
		/* truncate to 0														*/
		inode->i_size = 0;

		if( inode->i_blocks )
		{
			//DBGPRINT( "<ME2FS>%s:info:start truncateBlocks\n", __func__ );
			/* ---------------------------------------------------------------- */
			/* truncate blocks of disk											*/
			/* ---------------------------------------------------------------- */
			truncateBlocks( inode, 0 );
			//DBGPRINT( "<ME2FS>%s:info:end truncateBlocks\n", __func__ );
		}
	}

	invalidate_inode_buffers( inode );
	clear_inode( inode );

	if( want_delete )
	{
		//DBGPRINT( "<ME2FS>%s:info:start me2fsFreeInode\n", __func__ );
		me2fsFreeInode( inode );
		//DBGPRINT( "<ME2FS>%s:info:end me2fsFreeInode\n", __func__ );
		sb_end_intwrite( inode->i_sb );
	}
}

/*
----------------------------------------------------------------------------------

	Inode Operations for Ext2 Inode

----------------------------------------------------------------------------------
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsFiemap
	Input		:struct inode *inode
				 < vfs inode >
				 struct fiemap_extent_info *fieinfo
				 < file exent information >
				 u64 start
				 < initial block to map >
				 u64 len
				 < length of the extent to attempt to map >
	Output		:void
	Return		:int
				 < result >

	Description	:get file extent information
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsFiemap( struct inode*inode,
				 struct fiemap_extent_info *fieinfo,
				 u64 start,
				 u64 len )
{
	return( generic_block_fiemap( inode, fieinfo, start, len, me2fsGetBlock ) );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetAttr
	Input		:struct dentry *dentry
				 < vfs dentry >
				 iattr *iattr
				 < attribute of inode >
	Output		:void
	Return		:void

	Description	:set attribute to inode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsSetAttr( struct dentry *dentry, struct iattr *iattr )
{
	struct inode	*inode;
	int				error;

	inode = dentry->d_inode;

	DBGPRINT( "<ME2FS>%s:set attribute : name = %s, ino = %lu\n",
			  __func__, dentry->d_name.name, ( unsigned long )inode->i_ino );

	if( ( error = inode_change_ok( inode, iattr ) ) )
	{
		return( error );
	}

	/* as for now, quota is not implemented										*/

	if( ( iattr->ia_valid & ATTR_SIZE ) &&
		( iattr->ia_size != inode->i_size ) )
	{
		if( ( error = setInodeSize( inode, iattr->ia_size ) ) )
		{
			return( error );
		}
	}

	setattr_copy( inode, iattr );

	/* as for now, acl is not implemented										*/
	
	mark_inode_dirty( inode );

	return( error );
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
	DBGPRINT( "<ME2FS>i_no = %lu\n", ( unsigned long )inode->i_ino );
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
	DBGPRINT( "<ME2FS>i_no = %lu\n", ( unsigned long )mei->vfs_inode.i_ino ); 
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

	//DBGPRINT( "<ME2FS>%s:inode info:\n", __func__ );
	//DBGPRINT( "<ME2FS>ino=%lu iblock=%lu\n",
	//		  inode->i_ino, ( unsigned long )iblock );

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
	DBGPRINT( "vfs i_size = %lu\n", ( unsigned long )filp->f_inode->i_size );
	return( mpage_readpages( mapping, pages, nr_pages, me2fsGetBlock ) );
}
/*
==================================================================================
	Function	:me2fsWritePage
	Input		:struct page *page
				 < page cache >
				 struct writeback_control *wbc
				 < writeback control information >
	Output		:void
	Return		:int
				 < result >

	Description	:make a write back request of a page cache
==================================================================================
*/
static int me2fsWritePage( struct page *page, struct writeback_control *wbc )
{
	DBGPRINT( "<ME2FS>write page\n" );
	return( block_write_full_page( page, me2fsGetBlock, wbc ) );
}

/*
==================================================================================
	Function	:me2fsWriteBegin
	Input		:struct file *file
				 < vfs file object >
				 struct address_space *mapping
				 < address space asociated with the file >
				 loff_t pos
				 < position in a file >
				 unsigned len
				 < write size >
				 unsigned flags
				 < write flags >
				 struct page **pagep
				 < pages to be initialized >
				 void **fsdata
				 < file system specific data >
	Output		:void
	Return		:int
				 < result >

	Description	:prepare pages to write back
==================================================================================
*/
static int me2fsWriteBegin( struct file *file,
							struct address_space *mapping,
							loff_t pos,
							unsigned len,
							unsigned flags,
							struct page **pagep,
							void **fsdata )
{
	int		ret;

	DBGPRINT( "<ME2FS>write begin\n" );
	ret = block_write_begin( mapping, pos, len, flags, pagep, me2fsGetBlock );

	if( ret < 0 )
	{
		writeFailed( mapping, pos + len );
	}

	return( ret );

}

/*
==================================================================================
	Function	:me2fsWriteEnd
	Input		:struct file *file
				 < vfs file object >
				 struct address_space *mapping
				 < address space asociated with the file >
				 loff_t pos
				 < position in a file >
				 unsigned len
				 < write size >
				 unsigned copied
				 < copied number of byte >
				 struct page *page
				 < page to be comitted >
				 void **fsdata
				 < file system specific data >
	Output		:void
	Return		:int
				 < result >

	Description	:commit page cache write
==================================================================================
*/
static int me2fsWriteEnd( struct file *file,
						  struct address_space *mapping,
						  loff_t pos,
						  unsigned len,
						  unsigned copied,
						  struct page *page,
						  void *fsdata )
{
	int		ret;

	DBGPRINT( "<ME2FS>write end\n" );

	ret = generic_write_end( file, mapping, pos, len, copied, page, fsdata );

	if( ret < len )
	{
		writeFailed( mapping, pos + len );
	}

	return( ret );
}

/*
==================================================================================
	Function	:me2fsWritePages
	Input		:struct address_space *mapping
				 < address space ascociated with file >
				 struct writeback_control *wbc
				 < writeback control information >
	Output		:void
	Return		:int
				 < result >

	Description	:write page caches
==================================================================================
*/
static int me2fsWritePages( struct address_space *mapping,
							struct writeback_control *wbc )
{
	DBGPRINT( "<ME2FS>write page[s]\n" );
	DBGPRINT( "ino=%lu\n", mapping->host->i_ino );
	return( mpage_writepages( mapping, wbc, me2fsGetBlock ) );
}

/*
==================================================================================
	Function	:me2fsBmap
	Input		:struct address_space *mapping
				 < file mapping >
				 sector_t block
				 < block number >
	Output		:void
	Return		:void

	Description	:get block mapping
==================================================================================
*/
static sector_t me2fsBmap( struct address_space *mapping, sector_t block )
{
	return( generic_block_bmap( mapping, block, me2fsGetBlock ) );
}

/*
==================================================================================
	Function	:me2fsDirectIO
	Input		:int rw
				 < r/w direction >
				 struct kiocb *iocb
				 < i/o control information >
				 struct iov_iter *iter
				 < multiple user buffer >
				 loff_t offset
				 < offset >
	Output		:void
	Return		:ssize_t
				 < r/w result >

	Description	:do direct io
==================================================================================
*/
static ssize_t
me2fsDirectIO( int rw,
			   struct kiocb *iocb,
			   struct iov_iter *iter,
			   loff_t offset )
{
	struct file				*file;
	struct address_space	*mapping;
	struct inode			*inode;
	size_t					count;
	ssize_t					ret;

	DBGPRINT( "<ME2FS>:direct io\n" );

	file	= iocb->ki_filp;
	mapping	= file->f_mapping;
	inode	= mapping->host;

	count	= iov_iter_count( iter );

	ret = blockdev_direct_IO( rw,
							  iocb,
							  inode,
							  iter,
							  offset,
							  me2fsGetBlock );
	
	if( ( ret < 0 ) && ( rw & WRITE ) )
	{
		DBGPRINT( "<ME2FS>:direct io write failed\n" );
		writeFailed( mapping, offset + count );
	}

	return( ret );
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

	/* ------------------------------------------------------------------------ */
	/* direct block																*/
	/* ------------------------------------------------------------------------ */
	if( i_block < direct_blocks )
	{
		offsets[ depth++ ]	= i_block;
		final				= direct_blocks;
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

	
	//dbgPrintMe2fsInodeInfo( ME2FS_I(inode ) );

	addChain( chain, NULL, ME2FS_I( inode )->i_data + *offsets );

	if( !p->key )
	{
		DBGPRINT( "<ME2FS>%s:no block\n", __func__ );
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
		if( !verifyIndirectChain( chain, p ) )
		{
			goto truncated;
		}
		addChain( ++p, bh, ( __le32* )bh->b_data + *( ++offsets ) );
		read_unlock( &ME2FS_I( inode )->i_meta_lock );

		if( !p->key )
		{
			goto no_block;
		}
	}

	return( NULL );

	/* ------------------------------------------------------------------------ */
	/* detecte change															*/
	/* ------------------------------------------------------------------------ */
truncated:
	read_unlock( &( mei->i_meta_lock ) );
	brelse( bh );
	*err = -EAGAIN;

no_block:
	return( p );
}

/*
==================================================================================
	Function	:verifyIndirectChain
	Input		:Indirect *from
				 < indirect information to start verify >
				 Indirect *to
				 < indirect information to end verify >
	Output		:void
	Return		:int
				 < result >

	Description	:verify indirect chain
==================================================================================
*/
static inline int
verifyIndirectChain( Indirect *from, Indirect *to )
{
	while( ( from <= to ) && ( from->key == *from->p ) )
	{
		from++;
	}

	return( to < from );
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
	struct me2fs_inode_info	*mi;
	Indirect				chain[ 4 ];
	Indirect				*partial;
	int						offsets[ 4 ];
	int						blocks_to_boundary;
	int						count;
	int						depth;
	int						err;
	int						indirect_blks;
	unsigned long			goal;

	blocks_to_boundary = 0;

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

	err = -EIO;

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
			/* verify indirects chain											*/
			/* ---------------------------------------------------------------- */
			if( !verifyIndirectChain( chain, chain + depth - 1 ) )
			{
				/* ------------------------------------------------------------ */
				/* indirect block might be removed by truncate while			*/
				/* reading it. forget and  go to reread							*/
				/* ------------------------------------------------------------ */
				err		= -EAGAIN;
				count	= 0;
				break;
			}

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

	mi = ME2FS_I( inode );

	mutex_lock( &mi->truncate_mutex );

	/* ------------------------------------------------------------------------ */
	/* If the indirect block is missing while reading the chain, or				*/
	/* if the chain has benn changed after grabing the semaphore ,				*/
	/* (either because another process truncated this branch, or				*/
	/* another get_block allocated this branch) re-grab the chain to see if		*/
	/* the request block has been allocated or not								*/
	/*																			*/
	/* Since already blocking the truncate/other get_block at this point,		*/
	/* we will have the current copoy of the chain when we splice the branch	*/
	/* int the tree																*/
	/* ------------------------------------------------------------------------ */
	if( ( err == -EAGAIN ) || !verifyIndirectChain( chain, partial ) )
	{
		while( chain < partial )
		{
			brelse( partial->bh );
			partial--;
		}

		partial = me2fsGetBranch( inode, depth, offsets, chain, &err );

		if( !partial )
		{
			count++;
			mutex_unlock( &mi->truncate_mutex );
			if( err )
			{
				goto cleanup;
			}

			clear_buffer_new( bh_result );
			goto found;
		}
	}

	/* ------------------------------------------------------------------------ */
	/* NOW allocate blocks														*/
	/* ------------------------------------------------------------------------ */
	goal = findGoal( inode, iblock, partial );
	
	/* ------------------------------------------------------------------------ */
	/* the number of blocks need to allocate for [d,t]indirect blocks			*/
	/* ------------------------------------------------------------------------ */
	indirect_blks = ( chain + depth ) - partial - 1;

	/* ------------------------------------------------------------------------ */
	/* next, lookup the indirect map to count the total number of direct blocks	*/
	/* to allocate for this branch												*/
	/* ------------------------------------------------------------------------ */
	count = maxblocks;
	
	err		= allocBranch( inode,
						   indirect_blks,
						   &count,
						   goal,
						   offsets + ( partial - chain ),
						   partial );
	
	if( err )
	{
		DBGPRINT( "<ME2FS>%s:cannot allocate blocks in allocBranch\n",
				  __func__ );
		mutex_unlock( &mi->truncate_mutex );
		goto cleanup;
	}

	spliceBranch( inode, iblock, partial, indirect_blks, count );
	mutex_unlock( &mi->truncate_mutex );
	set_buffer_new( bh_result );

found:
	map_bh( bh_result, inode->i_sb, le32_to_cpu( chain[ depth - 1 ].key ) );
	/* i dont't care about boundary */
	err = count;

cleanup:
	DBGPRINT( "<ME2FS>%s:cleanup count = %d\n", __func__, count );
	while( chain < partial )
	{
		brelse( partial->bh );
		partial--;
	}
	return( err );

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
	Function	:__me2fsWriteInode
	Input		:struct inode *inode
				 < vfs inode >
				 int do_sync
				 < do sync flag >
	Output		:void
	Return		:int
				 < result >

	Description	:do write inode
==================================================================================
*/
static int
__me2fsWriteInode( struct inode *inode, int do_sync )
{
	struct me2fs_inode_info	*mi;
	struct super_block		*sb;
	ino_t					ino;
	uid_t					uid;
	gid_t					gid;
	struct buffer_head		*bh;
	struct ext2_inode		*ext2_inode;
	int						err;

	sb			= inode->i_sb;
	ino			= inode->i_ino;
	ext2_inode	= me2fsGetExt2Inode( sb, ino, &bh );

	if( IS_ERR( ext2_inode ) )
	{
		return( -EIO );
	}

	mi			= ME2FS_I( inode );
	uid			= i_uid_read( inode );
	gid			= i_gid_read( inode );
	err			= 0;

	/* ------------------------------------------------------------------------ */
	/* for fields not tracking in the in-memory inode,							*/
	/* initialize them to zero for new inodes									*/
	/* ------------------------------------------------------------------------ */
	if( mi->i_state & EXT2_STATE_NEW )
	{
		memset( ext2_inode, 0x00, ME2FS_SB( sb )->s_inode_size );
	}

	me2fsSetMe2fsInodeFlags( mi );

	ext2_inode->i_mode = cpu_to_le16( inode->i_mode );

	/* ------------------------------------------------------------------------ */
	/* 32-bit uid/gid															*/
	/* ------------------------------------------------------------------------ */
	if( !( ME2FS_SB( sb )->s_mount_opt & EXT2_MOUNT_NO_UID32 ) )
	{
		ext2_inode->i_uid = cpu_to_le16( low_16_bits( uid ) );
		ext2_inode->i_gid = cpu_to_le16( low_16_bits( gid ) );
		/* -------------------------------------------------------------------- */
		/* old inodes get re-used with the upper 16 bits of the uid/gid intact	*/
		/* -------------------------------------------------------------------- */
		if( !mi->i_dtime )
		{
			ext2_inode->osd2.linux2.l_i_uid_high
											= cpu_to_le16( high_16_bits( uid ) );
			ext2_inode->osd2.linux2.l_i_gid_high
											= cpu_to_le16( high_16_bits( gid ) );
		}
		else
		{
			ext2_inode->osd2.linux2.l_i_uid_high = 0;
			ext2_inode->osd2.linux2.l_i_gid_high = 0;
		}
	}
	/* ------------------------------------------------------------------------ */
	/* 16-bit uid/gid															*/
	/* ------------------------------------------------------------------------ */
	else
	{
		ext2_inode->i_uid	= cpu_to_le16( fs_high2lowuid( uid ) );
		ext2_inode->i_gid	= cpu_to_le16( fs_high2lowuid( gid ) );

		ext2_inode->osd2.linux2.l_i_uid_high	= 0;
		ext2_inode->osd2.linux2.l_i_gid_high	= 0;
	}

	ext2_inode->i_links_count	= cpu_to_le16( inode->i_nlink );
	ext2_inode->i_size			= cpu_to_le32( inode->i_size );
	ext2_inode->i_atime			= cpu_to_le32( inode->i_atime.tv_sec );
	ext2_inode->i_ctime			= cpu_to_le32( inode->i_ctime.tv_sec );
	ext2_inode->i_mtime			= cpu_to_le32( inode->i_mtime.tv_sec );
	
	ext2_inode->i_blocks		= cpu_to_le32( inode->i_blocks );
	ext2_inode->i_dtime			= cpu_to_le32( mi->i_dtime );
	ext2_inode->i_flags			= cpu_to_le32( mi->i_flags );
	ext2_inode->i_faddr			= cpu_to_le32( mi->i_faddr );
	ext2_inode->i_file_acl		= cpu_to_le32( mi->i_file_acl );

	ext2_inode->osd2.linux2.l_i_frag	= mi->i_frag_no;
	ext2_inode->osd2.linux2.l_i_fsize	= mi->i_frag_size;

	if( !S_ISREG( inode->i_mode ) )
	{
		ext2_inode->i_dir_acl = cpu_to_le32( mi->i_dir_acl );
	}
	else
	{
		/* i_size_high = i_dir_acl												*/
		ext2_inode->i_dir_acl = cpu_to_le32( inode->i_size >> 32 );

		if( 0x7FFFFFFFULL < inode->i_size )
		{
			if( !( ME2FS_SB( sb )->s_esb->s_feature_ro_compat
				 & cpu_to_le32( EXT2_FEATURE_RO_COMPAT_LARGE_FILE ) )
				|| ( ME2FS_SB( sb )->s_esb->s_rev_level
					 == cpu_to_le32( EXT2_GOOD_OLD_REV ) ) )
			{
				/* ------------------------------------------------------------ */
				/* if this is the first large file created, add a flag to		*/
				/* the super block												*/
				/* ------------------------------------------------------------ */
				me2fsWriteSuper( sb );
			}
		}
	}

	if( S_ISCHR( inode->i_mode ) || S_ISBLK( inode->i_mode ) )
	{
		if( old_valid_dev( inode->i_rdev ) )
		{
			ext2_inode->i_block[ 0 ] =
								cpu_to_le32( old_encode_dev( inode->i_rdev ) );
		}
		else
		{
			ext2_inode->i_block[ 0 ] = 0;
			ext2_inode->i_block[ 1 ] =
								cpu_to_le32( new_encode_dev( inode->i_rdev ) );
			ext2_inode->i_block[ 2 ] = 0;
		}
	}
	else
	{
		int	n;

		for( n = 0 ; n < ME2FS_NR_BLOCKS ; n++ )
		{
			ext2_inode->i_block[ n ] = mi->i_data[ n ];
		}
	}

	mark_buffer_dirty( bh );

	if( do_sync )
	{
		sync_dirty_buffer( bh );
		if( buffer_req( bh ) && !buffer_uptodate( bh ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:io error syncing inode[%s:%08lx]\n",
						 __func__, sb->s_id, ( unsigned long )ino );
			err = -EIO;
		}
	}

	mi->i_state &= ~EXT2_STATE_NEW;
	brelse( bh );

	return( err );
}

/*
==================================================================================
	Function	:allocBranch
	Input		:struct inode *inode
				 < vfs inode >
				 int indirect_blks
				 < indirect blocks >
				 int *blks
				 < blocks to allocate >
				 unsigned long goal
				 < block number of goal >
				 int *offsets
				 < offset of indirect information >
				 Indirect *branch
				 < indirect information >
	Output		:void
	Return		:int
				 < result >

	Description	:allocate blocks and link them into chain
==================================================================================
*/
static int allocBranch( struct inode *inode,
						int indirect_blks,
						int *blks,
						unsigned long goal,
						int *offsets,
						Indirect *branch )
{
	int					blocksize;
	int					i;
	int					ind_num;
	int					err;

	struct buffer_head	*bh;
	int					num;
	unsigned long		new_blocks[ 4 ];
	unsigned long		count;

	new_blocks[ 0 ] = me2fsNewBlocks( inode, goal, &count, &err );

	if( err )
	{
		return( err );
	}

	num = count;

	branch[ 0 ].key	= cpu_to_le32( new_blocks[ 0 ] );

	blocksize		= inode->i_sb->s_blocksize;

	/* ------------------------------------------------------------------------ */
	/* allocate metadata blocks and data blocks									*/
	/* ------------------------------------------------------------------------ */
	for( ind_num = 1 ; ind_num <= indirect_blks ; ind_num++ )
	{
		new_blocks[ ind_num ] = me2fsNewBlocks( inode, goal, &count, &err );
		if( err )
		{
			err = -ENOMEM;
			goto failed_new_blocks;
		}
		num++;
		/* -------------------------------------------------------------------- */
		/* get buffer head for parent block, zero it out and set the pointer	*/
		/* to the new one, then send parent to disk								*/
		/* -------------------------------------------------------------------- */
		bh = sb_getblk( inode->i_sb, new_blocks[ ind_num - 1 ] );
		if( unlikely( !bh ) )
		{
			err = -ENOMEM;
			goto failed;
		}
		branch[ ind_num ].bh	= bh;
		lock_buffer( bh );
		memset( bh->b_data, 0, blocksize );
		branch[ ind_num ].p		= ( __le32* )bh->b_data + offsets[ ind_num ];
		branch[ ind_num ].key	= cpu_to_le32( new_blocks[ ind_num ] );
		*branch[ ind_num ].p	= branch[ ind_num ].key;

		set_buffer_uptodate( bh );
		unlock_buffer( bh );
		mark_buffer_dirty_inode( bh, inode );
		/* -------------------------------------------------------------------- */
		/* we used to sync bh here if IS_SYNC(inode). but we now rely upon		*/
		/* generic_write_sync( ) and b_inode_buffers. but not for directories	*/
		/* -------------------------------------------------------------------- */
		if( S_ISDIR( inode->i_mode ) && IS_DIRSYNC( inode ) )
		{
			sync_dirty_buffer( bh );
		}
		
	}

	*blks = num;
	return( err );

failed_new_blocks:
	ind_num--;
failed:
	for( i = 1 ; i < ind_num ; i++ )
	{
		bforget( branch[ i ].bh );
	}

	for( i = 0 ; i < indirect_blks ; i++ )
	{
		/* as for now, i don't implement free blocks							*/
		//me2fsFreeBlocks( inode, new_blocks[ i ], 1 );
	}
	/* as for now, i don't implement free blocks								*/
	//me2fsFreeBlocks( inode, new_blocks[ i ], num );
	return( err );
}
/*
==================================================================================
	Function	:spliceBranch
	Input		:struct inode *inode
				 < vfs inode >
				 long block
				 < logical number of block to add >
				 Indirect *where
				 < location of missing link >
				 int num
				 < number of indirect blocks to add >
				 int blks
				 < number of direct blocks to add >
	Output		:Indirect *where
				 < location of missing link >
	Return		:void

	Description	:splice the allocated branch onto inode
==================================================================================
*/
static void spliceBranch( struct inode *inode,
						  long block,
						  Indirect *where,
						  int num,
						  int blks )
{
	*where->p = where->key;

	/* ------------------------------------------------------------------------ */
	/* update the host buffer_head or inode to point to more just allocated		*/
	/* blocks of direct blocks													*/
	/* ------------------------------------------------------------------------ */
	if( ( num == 0 ) && ( 1 < blks ) )
	{
		current_block = le32_to_cpu( where->key ) + 1;
		for( i = 1 ; i < blks ; i++ )
		{
			*( where->p + i ) = cpu_to_le32( current_block++ );
		}
	}

	if( where->bh )
	{
		mark_buffer_dirty_inode( where->bh, inode );
	}

	inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty( inode );
}

/*
==================================================================================
	Function	:findGoal
	Input		:struct inode *inode
				 < vfs inode >
				 long block
				 < block number >
				 Indirect *partial
				 < indirect information >
	Output		:void
	Return		:void

	Description	:find a preferred place for allocation
==================================================================================
*/
static inline unsigned long
findGoal( struct inode *inode, long block, Indirect *partial )
{
	return( findNear( inode, partial ) );
}

/*
==================================================================================
	Function	:findNear
	Input		:struct inode *inode
				 < vfs inode >
				 Indirect *ind
				 < indirect information >
	Output		:void
	Return		:unsigned long
				 < found block number >

	Description	:find a place for allocation with sufficient locality
==================================================================================
*/
static unsigned long
findNear( struct inode *inode, Indirect *ind )
{
	struct me2fs_inode_info	*mei;
	struct me2fs_sb_info	*msi;
	__le32					*start;
	__le32					*cur;
	unsigned long			bg_start;
	unsigned long			color;

	mei = ME2FS_I( inode );

	if( ind->bh )
	{
		start = ( __le32* )ind->bh->b_data;
	}
	else
	{
		start = mei->i_data;
	}

	/* ------------------------------------------------------------------------ */
	/* try to find previous block												*/
	/* ------------------------------------------------------------------------ */
	for( cur = ind->p - 1 ; start <= cur ; cur-- )
	{
		if( *cur )
		{
			return( le32_to_cpu( *cur ) );
		}
	}

	/* ------------------------------------------------------------------------ */
	/* no such thing, so let's try location of indirect block					*/
	/* ------------------------------------------------------------------------ */
	if( ind->bh )
	{
		return( ind->bh->b_blocknr );
	}

	/* ------------------------------------------------------------------------ */
	/* it is going to be referred from inode itself? then, just put it into		*/
	/* the same cylinder group.													*/
	/* ------------------------------------------------------------------------ */
	msi			= ME2FS_SB( inode->i_sb );
	bg_start	= ext2GetFirstBlockNum( inode->i_sb, mei->i_block_group );
	color		= ( current->pid % 16 ) * ( msi->s_blocks_per_group / 16 );

	return( bg_start + color );
}
/*
==================================================================================
	Function	:isInodeFastSymlink
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:int
				 < result >

	Description	:test whether the given inode is fast symlink or not
==================================================================================
*/
static inline int
isInodeFastSymlink( struct inode *inode )
{
	int		ea_blocks;

	if( ME2FS_I( inode )->i_file_acl )
	{
		ea_blocks = inode->i_sb->s_blocksize >> 9;
	}
	else
	{
		ea_blocks = 0;
	}

	return( S_ISLNK( inode->i_mode ) &&
			( ( inode->i_blocks - ea_blocks ) == 0 ) );
}

/*
==================================================================================
	Function	:truncateBlocks
	Input		:struct inode *inode
				 < vfs inode to truncate >
				 loff_t offset
				 < offset to start truncation >
	Output		:void
	Return		:void

	Description	:truncate blocks of me2fs filesystem
==================================================================================
*/
static void truncateBlocks( struct inode *inode, loff_t offset )
{
	if( !( S_ISREG( inode->i_mode ) ||
		   S_ISDIR( inode->i_mode ) ||
		   S_ISLNK( inode->i_mode ) ) )
	{
		return;
	}

	if( isInodeFastSymlink( inode ) )
	{
		return;
	}

	if( IS_APPEND( inode ) || IS_IMMUTABLE( inode ) )
	{
		return;
	}

	__truncateBlocks( inode, offset );
}
/*
==================================================================================
	Function	:__truncateBlocks
	Input		:struct inode *inode
				 < vfs inode to truncate >
				 loff_t offset
				 < offset to start truncation >
	Output		:void
	Return		:void

	Description	:core function of truncating blocks
==================================================================================
*/
static void __truncateBlocks( struct inode *inode, loff_t offset )
{
	__le32					*i_data;
	struct me2fs_inode_info	*mi;
	int						addr_per_block;
	int						offsets[ 4 ];
	Indirect				chain[ 4 ];
	Indirect				*partial;
	__le32					nr;
	int						depth;
	long					iblock;
	unsigned				blocksize;

	blocksize	= inode->i_sb->s_blocksize;
	iblock		= ( offset + blocksize - 1 )
				  >> inode->i_sb->s_blocksize_bits;
	
	depth = me2fsBlockToPath( inode, iblock, offsets, NULL );

	if( depth == 0 )
	{
		return;
	}

	/* ------------------------------------------------------------------------ */
	/* from here we block out all me2fsGetBlock() callers who want to modify	*/
	/* the block allocation tree												*/
	/* ------------------------------------------------------------------------ */
	mi				= ME2FS_I( inode );
	i_data			= mi->i_data;
	addr_per_block	= inode->i_sb->s_blocksize / sizeof( __u32 );
	mutex_lock( &mi->truncate_mutex );

	if( depth == 1 )
	{
		freeData( inode, i_data + offsets[ 0 ], i_data + ME2FS_NDIR_BLOCKS );
		goto do_indirects;
	}

	partial = findShared( inode, depth, offsets, chain, &nr );

	/* ------------------------------------------------------------------------ */
	/* kill the top of shared branch (already detached)							*/
	/* ------------------------------------------------------------------------ */
	if( nr )
	{
		if( partial == chain )
		{
			mark_inode_dirty( inode );
		}
		else
		{
			mark_buffer_dirty_inode( partial->bh, inode );
		}

		freeBranches( inode, &nr, &nr + 1, ( chain + depth - 1 ) - partial );
	}

	/* ------------------------------------------------------------------------ */
	/* clear the ends of indirect blocks on the shared branch					*/
	/* ------------------------------------------------------------------------ */
	while( chain < partial )
	{
		freeBranches( inode,
					  partial->p + 1,
					  ( __le32* )partial->bh->b_data + addr_per_block,
					  ( chain + depth - 1 ) - partial );
		mark_buffer_dirty_inode( partial->bh, inode );
		brelse( partial->bh );
		partial--;
	}

do_indirects:
	/* ------------------------------------------------------------------------ */
	/* kill the remaining (whole) subtrees										*/
	/* ------------------------------------------------------------------------ */
	switch( offsets[ 0 ] )
	{
	default:
		nr = i_data[ ME2FS_IND_BLOCK ];
		if( nr )
		{
			i_data[ ME2FS_IND_BLOCK ] = 0;
			mark_inode_dirty( inode );
			freeBranches( inode, &nr, &nr + 1, 1 );
		}
		/* go through															*/
	case	ME2FS_IND_BLOCK:
		nr = i_data[ ME2FS_2IND_BLOCK ];
		if( nr )
		{
			i_data[ ME2FS_2IND_BLOCK ] = 0;
			mark_inode_dirty( inode );
			freeBranches( inode, &nr, &nr + 1, 2 );
		}
		/* go through															*/
	case	ME2FS_2IND_BLOCK:
		nr = i_data[ ME2FS_3IND_BLOCK ];
		if( nr )
		{
			i_data[ ME2FS_2IND_BLOCK ] = 0;
			mark_inode_dirty( inode );
			freeBranches( inode, &nr, &nr + 1, 3 );
		}
		/* go through															*/
	case	ME2FS_3IND_BLOCK:
		break;
	}
	
	mutex_unlock( &mi->truncate_mutex );
}
/*
==================================================================================
	Function	:freeData
	Input		:struct inode *inode
				 < vfs inode to free data >
				 __le32 *blocks
				 < array of block numbers >
				 __le32 *end_block
	Output		:void
	Return		:void

	Description	:free a list of data blocks
==================================================================================
*/
static inline void
freeData( struct inode *inode, __le32 *blocks, __le32 *end_block )
{
	unsigned long	block_to_free;
	unsigned long	count;
	unsigned long	block_num;

	block_to_free	= 0;
	count			= 0;

	for( ; blocks < end_block ; blocks++ )
	{
		if( *blocks )
		{
			block_num	= le32_to_cpu( *blocks );
			*blocks		= 0;

			/* ---------------------------------------------------------------- */
			/* accumulate blocks to free if they are contiguous					*/
			/* ---------------------------------------------------------------- */
			if( count == 0 )
			{
				block_to_free	= block_num;
				count			= 1;
			}
			else if( block_to_free == ( block_num - count ) )
			{
				count++;
			}
			else
			{
				me2fsFreeBlocks( inode, block_to_free, count );
				mark_inode_dirty( inode );
				block_to_free	= block_num;
				count			= 1;
			}
		}
	}

	if( 0 < count )
	{
		me2fsFreeBlocks( inode, block_to_free, count );
		mark_inode_dirty( inode );
	}
}
/*
==================================================================================
	Function	:findShared
	Input		:struct inode *inode
				 < vfs inode >
				 int depth
				 < depth of indirects >
				 int offsets[ 4 ]
				 < offsets of pointers in that branch >
				 Indirect chain[ 4 ]
				 < indirets tree information >
				 __le32 *top
				 < detached top of branch >
	Output		:void
	Return		:Indirect*
				 < partial indirect information >

	Description	:find the indirect blocks for partial truncation
==================================================================================
*/
static Indirect*
findShared( struct inode *inode,
			int depth,
			int offsets[ 4 ],
			Indirect chain[ 4 ],
			__le32 *top )
{
	Indirect	*partial;
	Indirect	*cur;
	int			last_ind;
	int			err;

	*top		= 0;
	last_ind	= depth;
	while( ( 1 < last_ind ) && !offsets[ last_ind - 1 ] )
	{
		last_ind--;
	}

	partial = me2fsGetBranch( inode, last_ind, offsets, chain, &err );

	if( !partial )
	{
		partial = chain + last_ind - 1;
	}

	/* ------------------------------------------------------------------------ */
	/* if the branch aquired continuation since we've looked at it fine, it		*/
	/* should all survive and (new) top doesn't belong to us					*/
	/* ------------------------------------------------------------------------ */
	write_lock( &ME2FS_I( inode )->i_meta_lock );
	if( !partial->key && *partial->p )
	{
		write_unlock( &ME2FS_I( inode )->i_meta_lock );

		/* no top																*/
		return( partial );
	}

	cur = partial;
	while( ( chain < cur ) &&
		   searchFirstNonZero( ( __le32* )cur->bh->b_data, cur->p ) )
	{
		cur--;
	}

	/* ------------------------------------------------------------------------ */
	/* ok, we've found the last block that must survive. the rest of our branch	*/
	/* should be detached before unlocking. however, if that rest of branch is 	*/
	/* all ours and does not grow immediately from the inode it's easier to		*/
	/* cheat and just decrement partial->p										*/
	/* ------------------------------------------------------------------------ */
	if( ( cur == ( chain + last_ind - 1 ) ) &&
		( chain < cur ) )
	{
		cur->p--;
	}
	else
	{
		*top	= *cur->p;
		*cur->p	= 0;
	}

	write_unlock( &ME2FS_I( inode )->i_meta_lock );

	while( cur < partial )
	{
		brelse( partial->bh );
		partial--;
	}

	return( partial );
}

/*
==================================================================================
	Function	:searchFirstNonZero
	Input		:__le32 *cur
				 < search point >
				 __le32 *end
				 < end point >
	Output		:void
	Return		:int
				 < result >

	Description	:search first non zero word
==================================================================================
*/
static inline int
searchFirstNonZero( __le32 *cur, __le32 *end )
{
	while( cur < end )
	{
		if( *cur++ )
		{
			return( 0 );
		}
	}
	return( 1 );
}
/*
==================================================================================
	Function	:freeBranches
	Input		:struct inode *inode
				 < host vfs inode >
				 __le32 *cur
				 < array of block numbers >
				 __le32 *end
				 < pointer immediately past the end of array >
				 int depth
				 < depth of the branches to free >
	Output		:void
	Return		:void

	Description	:free an array of branches
==================================================================================
*/
static void
freeBranches( struct inode *inode,
			  __le32 *cur,
			  __le32 *end,
			  int depth )
{
	struct buffer_head	*bh;
	unsigned long		nr;

	if( depth-- )
	{
		int	addr_per_block;

		addr_per_block = inode->i_sb->s_blocksize / sizeof( __u32 );

		for( ; cur < end ; cur++ )
		{
			if( !( nr = le32_to_cpu( *cur ) ) )
			{
				continue;
			}

			*cur = 0;

			if( !( bh = sb_bread( inode->i_sb, nr ) ) )
			{
				ME2FS_ERROR( "<ME2FS>%s:error:sb_read inode=%ld, block=%ld\n",
							 __func__, inode->i_ino, nr );
				continue;
			}

			freeBranches( inode,
						  ( __le32* )bh->b_data,
						  ( __le32* )bh->b_data + addr_per_block,
						  depth );

			bforget( bh );
			me2fsFreeBlocks( inode, nr, 1 );
			mark_inode_dirty( inode );
		}
	}
	else
	{
		freeData( inode, cur, end );
	}
}

/*
==================================================================================
	Function	:setInodeSize
	Input		:struct inode *inode
				 < vfs inode >
				 loff_t newsize
				 < new size of a file >
	Output		:void
	Return		:void

	Description	:set size
==================================================================================
*/
static int setInodeSize( struct inode *inode, loff_t newsize )
{
	int	error;

	if( !( S_ISREG( inode->i_mode )	||
		S_ISDIR( inode->i_mode )	||
		S_ISLNK( inode->i_mode ) ) )
	{
		return( -EINVAL );
	}

	if( isInodeFastSymlink( inode ) )
	{
		return( -EINVAL );
	}

	if( IS_APPEND( inode ) || IS_IMMUTABLE( inode ) )
	{
		return( -EPERM );
	}

	inode_dio_wait( inode );

#if 0	// as for now, xip is not implemented
	if( mapping_is_xip( inode->i_mapping ) )
	{
		error = xip_truncate_page( inode->i_mapping, newsize );
	}
	else if( ME2FS_SB( inode->i_sb )->s_mount_opt & EXT2_MOUNT_NOBH )
#endif
	if( ME2FS_SB( inode->i_sb )->s_mount_opt & EXT2_MOUNT_NOBH )
	{
		error = nobh_truncate_page( inode->i_mapping, newsize, me2fsGetBlock );
	}
	else
	{
		error = block_truncate_page( inode->i_mapping, newsize, me2fsGetBlock );
	}

	if( error )
	{
		return( error );
	}

	truncate_setsize( inode, newsize );
	__truncateBlocks( inode, newsize );

	inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_mtime = inode->i_ctime;

	if( inode_needs_sync( inode ) )
	{
		sync_mapping_buffers( inode->i_mapping );
		sync_inode_metadata( inode, 1 );
	}
	else
	{
		mark_inode_dirty( inode );
	}

	return( 0 );
}
/*
==================================================================================
	Function	:writeFailed
	Input		:struct address_space *mapping
				 < file mapping >
				 loff_t size
				 < size of write attemption >
	Output		:void
	Return		:void

	Description	:truncate blocks after write failed
==================================================================================
*/
static void writeFailed( struct address_space *mapping, loff_t size )
{
	struct inode	*inode;

	inode = mapping->host;

	if( inode->i_size < size )
	{
		truncate_pagecache( inode, inode->i_size );
		truncateBlocks( inode, inode->i_size );
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
