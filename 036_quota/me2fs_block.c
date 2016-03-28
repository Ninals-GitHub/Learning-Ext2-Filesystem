/********************************************************************************
	File			: me2fs_block.c
	Description		: management blocks of my ext2 file system

*********************************************************************************/
#include <linux/sched.h>
#include <linux/buffer_head.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/quotaops.h>

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
static inline int
isRsvEmpty( struct ext2_reserve_window *rsv );
static unsigned long
tryToAllocateWithRsv( struct super_block *sb,
					  unsigned int group,
					  struct buffer_head *bitmap_bh,
					  unsigned long grp_goal,
					  struct ext2_reserve_window_node *my_rsv,
					  unsigned long *count );
static int
goalInMyReservation( struct ext2_reserve_window *rsv,
					 unsigned long grp_goal,
					 unsigned int group,
					 struct super_block *sb );
static int
allocNewReservation( struct ext2_reserve_window_node *my_rsv,
					 unsigned long grp_goal,
					 struct super_block *sb,
					 unsigned int group,
					 struct buffer_head *bitmap_bh );
static void
tryToExtendReservation( struct ext2_reserve_window_node *my_rsv,
						struct super_block *sb,
						int size );
static unsigned long
searchBitmapNextUsableBlock( unsigned long start,
							 struct buffer_head *bh,
							 unsigned long end );
static struct ext2_reserve_window_node*
searchReserveWindow( struct rb_root *root, unsigned long goal );
static void removeReserveWindow( struct super_block *sb,
								 struct ext2_reserve_window_node *rsv );
static void
tryToExtendReservation( struct ext2_reserve_window_node *my_rsv,
						struct super_block *sb,
						int size );
static int
findNextReservableWindow( struct ext2_reserve_window_node *search_head,
						  struct ext2_reserve_window_node *my_rsv,
						  struct super_block *sb,
						  unsigned long start_block,
						  unsigned long last_block );
static unsigned long
findNextUsableBlock( int start, struct buffer_head *bh, int end );

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
	unsigned long			ngroups;
	int						bgi;
	int						performed_allocation;
	int						ret;
	
	struct ext2_block_alloc_info	*block_i;
	struct ext2_reserve_window_node	*my_rsv;
	unsigned short					windowsz;

	/* ------------------------------------------------------------------------ */
	/* check quota for allocation of this block									*/
	/* ------------------------------------------------------------------------ */
	ret = dquot_alloc_block( inode, *count );

	if( ret )
	{
		*err = ret;
		return( 0 );
	}

	sb			= inode->i_sb;
	msi			= ME2FS_SB( sb );
	esb			= msi->s_esb;

	bitmap_bh	= NULL;
	my_rsv		= NULL;
	windowsz	= 0;

	ret_block	= 0;
	num			= *count;
	
	performed_allocation = 0;

	/* ------------------------------------------------------------------------ */
	/* allocate a block from reservation (the filesystem always use reservetion	*/
	/* window). it' a regular file, and the desired window size if greater than	*/
	/* 0 (one could user ioctl command EXT2_IOC_SETRSVSZ to set the window size	*/
	/* to 0 to turn off reservation on that particular file)					*/
	/* ------------------------------------------------------------------------ */
	block_i = ME2FS_I( inode )->i_block_alloc_info;
	if( block_i )
	{
		windowsz = block_i->rsv_window_node.rsv_goal_size;
		if( 0 < windowsz )
		{
			my_rsv = &block_i->rsv_window_node;
		}
	}

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

retry_alloc:
	if( !( gdesc = me2fsGetGroupDescriptor( sb, group_no ) ) )
	{
		goto io_error;
	}

	if( !( gdesc_bh = me2fsGetGdescBufferCache( sb, group_no ) ) )
	{
		goto io_error;
	}

	free_blocks = le16_to_cpu( gdesc->bg_free_blocks_count );

	/* ------------------------------------------------------------------------ */
	/* if there is not enough free blocks to make a new reservation, turn off	*/
	/* reservation for this allocation											*/
	/* ------------------------------------------------------------------------ */
	if( my_rsv &&
		( free_blocks < windowsz ) &&
		( isRsvEmpty( &my_rsv->rsv_window ) ) )
	{
		my_rsv = NULL;
	}

	if( 0 < free_blocks )
	{
		grp_target_blk	= ( goal - le32_to_cpu( esb->s_first_data_block ) )
						  % msi->s_blocks_per_group;
		bitmap_bh		= readBlockBitmap( sb, group_no );

		if( !bitmap_bh )
		{
			goto io_error;
		}

#if 0
		grp_alloc_blk = tryToAllocate( sb,
									   group_no,
									   bitmap_bh,
									   grp_target_blk,
									   count,
									   NULL );
#endif
		grp_alloc_blk = tryToAllocateWithRsv( sb,
											  group_no,
											  bitmap_bh,
											  grp_target_blk,
											  my_rsv,
											  &num );

		if( 0 <= grp_alloc_blk )
		{
			goto allocated;
		}
	}
#if 0
	/* ------------------------------------------------------------------------ */
	/* as for now, i do not implement free block search							*/
	/* ------------------------------------------------------------------------ */
	else
	{
		*err = -ENOSPC;
		goto out;
	}
#endif
	
	ngroups = msi->s_groups_count;
	smp_rmb( );

	/* ------------------------------------------------------------------------ */
	/* Now search the rest of the group. we assume that group_no and gdesc		*/
	/* correctly point to the last group visited.								*/
	/* ------------------------------------------------------------------------ */
	for( bgi = 0 ; bgi < ngroups ; bgi++ )
	{
		group_no++;
		if( ngroups <= group_no )
		{
			group_no = 0;
		}

		if( !( gdesc = me2fsGetGroupDescriptor( sb, group_no ) ) )
		{
			goto io_error;
		}

		if( !( gdesc_bh = me2fsGetGdescBufferCache( sb, group_no ) ) )
		{
			goto io_error;
		}

		free_blocks = le16_to_cpu( gdesc->bg_free_blocks_count );
		/* -------------------------------------------------------------------- */
		/* skip this group if the number of free blocks is less than half of	*/
		/* the reservation window size.											*/
		/* -------------------------------------------------------------------- */
		if( my_rsv && ( free_blocks <= ( windowsz / 2 ) ) )
		{
			continue;
		}

		brelse( bitmap_bh );
		if( !( bitmap_bh = readBlockBitmap( sb, group_no ) ) )
		{
			goto io_error;
		}

		/* -------------------------------------------------------------------- */
		/* try to allocate block(s) from this group, without a goal - 1			*/
		/* -------------------------------------------------------------------- */
		grp_alloc_blk = tryToAllocateWithRsv( sb,
											  group_no,
											  bitmap_bh,
											  -1,
											  my_rsv,
											  &num );

		if( 0 <= grp_alloc_blk )
		{
			goto allocated;
		}
	}

	/* ------------------------------------------------------------------------ */
	/* we may end up a bogus earlie ENOSPC error due to filesystem is "full"	*/
	/* of reservation, but there maybe indeed free blocks available on disk		*/
	/* in this case, we just forget about the reservations just do block		*/
	/* allocaton as without reservations.										*/
	/* ------------------------------------------------------------------------ */
	if( my_rsv )
	{
		my_rsv		= NULL;
		windowsz	= 0;
		group_no	= goal_group;
		goto retry_alloc;
	}

	/* ------------------------------------------------------------------------ */
	/* no space left on the device												*/
	/* ------------------------------------------------------------------------ */
	*err = -ENOSPC;
	goto out;

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
		/* tryToAllocate() marked the blocks we allocated as in user. So we		*/
		/* may want to selectively mark some of the blocks as free				*/
		/* -------------------------------------------------------------------- */
		goto retry_alloc;
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
		dquot_free_block_nodirty( inode, *count - num );
		mark_inode_dirty( inode );
		*count = num;
	}
	
	return( ret_block );

io_error:
	*err = -EIO;
out:
	if( !performed_allocation )
	{
		dquot_free_block_nodirty( inode, *count );
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
	Function	:me2fsInitBlockAllocInfo
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:allocate block_alloc_info
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsInitBlockAllocInfo( struct inode *inode )
{
	struct ext2_block_alloc_info	*alloc_info;

	alloc_info = kmalloc( sizeof( *alloc_info ), GFP_NOFS );

	if( alloc_info )
	{
		struct ext2_reserve_window_node	*rsv;

		rsv = &alloc_info->rsv_window_node;

		rsv->rsv_start		= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
		rsv->rsv_end		= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
		rsv->rsv_goal_size	= EXT2_DEFAULT_RESERVE_BLOCKS;
		rsv->rsv_alloc_hit	= 0;
		
		alloc_info->last_alloc_logical_block	= 0;
		alloc_info->last_alloc_physical_block	= 0;
	}

	ME2FS_I( inode )->i_block_alloc_info = alloc_info;
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInsertReserveWindow
	Input		:struct superbe_block *sb
				 < vfs super block >
				 struct ext2_reserve_window_node *rsv
				 < reservation window node to add >
	Output		:void
	Return		:void

	Description	:insert a window to the block reservation rb tree
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsInsertReserveWindow( struct super_block *sb,
							   struct ext2_reserve_window_node *rsv )
{
	struct rb_root					*root;
	struct rb_node					*node;
	unsigned long					start;

	struct rb_node					**p;
	struct rb_node					*parent;
	struct ext2_reserve_window_node	*this;

	root	= &ME2FS_SB( sb )->s_rsv_window_root;
	node	= &rsv->rsv_node;
	
	start	= rsv->rsv_start;

	p		= &root->rb_node;
	parent	= NULL;

	while( *p )
	{
		parent = *p;

		this = rb_entry( parent, struct ext2_reserve_window_node, rsv_node );

		if( start < this->rsv_start )
		{
			p = &( ( *p )->rb_left );
		}
		else if( this->rsv_end < start )
		{
			p = &( ( *p )->rb_right );
		}
		else
		{
			DBGPRINT( "<ME2FS>%s:bug on\n", __func__ );
		}
	}

	rb_link_node( node, parent, p );
	rb_insert_color( node, root );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsDiscardReservation
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:free block reservation window on last file close, or truncate
				 or at last input()
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsDiscardReservation( struct inode *inode )
{
	struct me2fs_inode_info			*mei;
	struct ext2_block_alloc_info	*block_i;
	struct ext2_reserve_window_node	*rsv;
	spinlock_t						*rsv_lock;

	mei	= ME2FS_I( inode );
	
	if( !( block_i = mei->i_block_alloc_info ) )
	{
		return;
	}

	rsv_lock	= &ME2FS_SB( inode->i_sb )->s_rsv_window_lock;
	rsv			= &block_i->rsv_window_node;

	if( !isRsvEmpty( &rsv->rsv_window ) )
	{
		spin_lock( rsv_lock );
		{
			if( !isRsvEmpty( &rsv->rsv_window ) )
			{
				removeReserveWindow( inode->i_sb, rsv );
			}
		}
		spin_unlock( rsv_lock );
	}
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
	unsigned long	group_first_block;
	unsigned long	start;
	unsigned long	end;
	unsigned long	num;

	num		= 0;

	/* ------------------------------------------------------------------------ */
	/* we do allocation within the reservation window if we have a window		*/
	/* ------------------------------------------------------------------------ */
	if( my_rsv )
	{
		group_first_block = ext2GetFirstBlockNum( sb, group );
		if( group_first_block <= my_rsv->_rsv_start )
		{
			start = my_rsv->_rsv_start - group_first_block;
		}
		else
		{
			/* reservation window crosses group boundary						*/
			start = 0;
		}

		end = my_rsv->_rsv_end - group_first_block + 1;

		if( ME2FS_SB( sb )->s_blocks_per_group < end )
		{
			/* reservation window crosses group boudary							*/
			end = ME2FS_SB( sb )->s_blocks_per_group;
		}

		if( ( start <= grp_goal ) && ( grp_goal < end ) )
		{
			start = grp_goal;
		}
		else
		{
			grp_goal = -1;
		}
	}
	else
	{
		if( 0 < grp_goal )
		{
			start = grp_goal;
		}
		else
		{
			start = 0;
		}

		end = ME2FS_SB( sb )->s_blocks_per_group;
	}

repeat:
	if( grp_goal < 0 )
	{
		grp_goal = findNextUsableBlock( start, bitmap_bh, end );
		
		if( grp_goal < 0 )
		{
			goto fail_access;
		}

		if( !my_rsv )
		{
			int	i;

			for( i = 0 ;
				 ( i < 7 ) &&
				 ( start < grp_goal ) &&
				 !test_bit_le( grp_goal - 1, bitmap_bh->b_data ) ;
				 i++, grp_goal-- )
			{
				/* loop with doing nothing										*/
			}
		}
	}

	start = grp_goal;

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
	grp_goal++;

	while( ( num < *count ) &&
		   ( grp_goal < end ) &&
		   !ext2_set_bit_atomic( getSbBlockGroupLock( ME2FS_SB( sb ), group ),
		   						 grp_goal,
								 bitmap_bh->b_data ) )
	{
		num++;
		grp_goal++;
	}

	*count = num;
	return( grp_goal - num );

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
	Function	:isRsvEmpty
	Input		:struct ext2_reserve_window *rsv
				 < reservation window to check >
	Output		:void
	Return		:int
				 < result >

	Description	:check if the reservation window is allocated
==================================================================================
*/
static inline int
isRsvEmpty( struct ext2_reserve_window *rsv )
{
	/* ------------------------------------------------------------------------ */
	/* a valid reservation end block could not be 0								*/
	/* ------------------------------------------------------------------------ */
	return( rsv->_rsv_end == EXT2_RESERVE_WINDOW_NOT_ALLOCATED );
}
/*
==================================================================================
	Function	:tryToAllocateWithRsv
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int group
				 < group number to allocate block >
				 struct buffer_head *bitmap_bh
				 < buffer cache for bitmap >
				 unsigned long grp_goal
				 < goal of group >
				 struct ext2_reserve_window_node *my_rsv
				 < the windwo >
				 unsigned long *count
				 < number of blocks to allocate >
	Output		:void
	Return		:unsigned long
				 < number of allocated blocks >

	Description	:allocate new block with reservation window
==================================================================================
*/
static unsigned long
tryToAllocateWithRsv( struct super_block *sb,
					  unsigned int group,
					  struct buffer_head *bitmap_bh,
					  unsigned long grp_goal,
					  struct ext2_reserve_window_node *my_rsv,
					  unsigned long *count )
{
	unsigned long	group_first_block;
	unsigned long	group_last_block;
	unsigned long	ret;
	unsigned long	num;

	ret = 0;
	num = *count;


	/* ------------------------------------------------------------------------ */
	/* we don't deal with reservation when the file is not a regular file or	*/
	/* last attempt to allocate a block with reservation turned on failed		*/
	/* ------------------------------------------------------------------------ */
	if( !my_rsv )
	{
		return( tryToAllocate( sb, group, bitmap_bh, grp_goal, count, NULL ) );
	}

	/* ------------------------------------------------------------------------ */
	/* grp_goal is a group relative block number (if there is a goal)			*/
	/* 0 <= grp_goal < msi->s_blocks_per_group									*/
	/* first block is a filesystem wide block number							*/
	/* first block is the block number of the first block in this group			*/
	/* ------------------------------------------------------------------------ */
	group_first_block	= ext2GetFirstBlockNum( sb, group );
	group_last_block	= group_first_block +
						  ( ME2FS_SB( sb )->s_blocks_per_group -1 );

	/* ------------------------------------------------------------------------ */
	/* basicall we will allocate a new block from inode's reservation window.	*/
	/* we need to allocate a new reservation window, if:						*/
	/* a) inode does not have a reservation winow; or							*/
	/* b) last attempt to allocate a block from existing reservation failed; or	*/
	/* c) we come here with a goal and with a reservation window				*/
	/*																			*/
	/* we do not need to allocate a new reservation window if we come here at	*/
	/* the beginning with a goal and the goal is inside the window, or we don't	*/
	/* have a goal but already have a reservation window. then we could go to	*/
	/* allocate from the reservation window directly.							*/
	/* ------------------------------------------------------------------------ */
	while( 1 )
	{
		if( isRsvEmpty( &my_rsv->rsv_window ) ||
			( ret < 0 ) ||
			!goalInMyReservation( &my_rsv->rsv_window,
								  grp_goal,
								  group,
								  sb ) )
		{
			if( my_rsv->rsv_goal_size < *count )
			{
				my_rsv->rsv_goal_size = *count;
			}

			ret = allocNewReservation( my_rsv, grp_goal, sb, group, bitmap_bh );

			if( ret < 0 )
			{
				/* failed														*/
				break;
			}

			if( !goalInMyReservation( &my_rsv->rsv_window,
									  grp_goal,
									  group,
									  sb ) )
			{
				grp_goal = -1;
			}
		}
		else if( 0 <= grp_goal )
		{
			int curr;

			curr = my_rsv->rsv_end - ( grp_goal + group_first_block ) + 1;

			if( curr < *count )
			{
				tryToExtendReservation( my_rsv, sb, *count - curr );
			}
		}

		if( ( group_last_block < my_rsv->rsv_start ) ||
			( my_rsv->rsv_end < group_first_block ) )
		{
			DBGPRINT( "<ME2FS>%s:window bug\n", __func__ );
		}

		ret = tryToAllocate( sb,
							 group,
							 bitmap_bh,
							 grp_goal,
							 &num,
							 &my_rsv->rsv_window );

		if( 0 <= ret )
		{
			/* succeed															*/
			my_rsv->rsv_alloc_hit += num;
			*count = num;
			break;
		}

		num = *count;
	}
	
	return( ret );
}

/*
==================================================================================
	Function	:goalInMyReservation
	Input		:struct ext2_reserve_window *rsv
				 < window information >
				 unsigned long grp_goal
				 < goal of group >
				 unsigned int group
				 < current allocation group number >
				 struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:int
				 < result >

	Description	:test if the goal block (group relative) is within the file's
				 own block reservation window range
==================================================================================
*/
static int
goalInMyReservation( struct ext2_reserve_window *rsv,
					 unsigned long grp_goal,
					 unsigned int group,
					 struct super_block *sb )
{
	unsigned long	group_first_block;
	unsigned long	group_last_block;

	group_first_block	= ext2GetFirstBlockNum( sb, group );
	group_last_block	= group_first_block +
						  ME2FS_SB( sb )->s_blocks_per_group - 1;
	
	if( ( group_last_block < rsv->_rsv_start ) ||
		( rsv->_rsv_end < group_first_block ) )
	{
		return( 0 );
	}

	if( ( 0 <= grp_goal ) &&
		( ( ( grp_goal + group_first_block ) < rsv->_rsv_start ) ||
		  ( rsv->_rsv_end < ( grp_goal + group_first_block ) ) ) )
	{
		return( 0 );
	}

	return( 1 );
}
/*
==================================================================================
	Function	:allocNewReservation
	Input		:struct ext2_reserve_window_node *my_rsv
				 < the window >
				 unsigned long grp_goal
				 < the goal (group-relative) >
				 struct super_block *sb
				 < vfs super block >
				 unsigned int group
				 < group number to allocate in >
				 struct buffer_head *bitmap_bh
				 < buffer cache for bitmap >
	Output		:void
	Return		:int
				 < result >

	Description	:allocate a new reservation window
==================================================================================
*/
static int
allocNewReservation( struct ext2_reserve_window_node *my_rsv,
					 unsigned long grp_goal,
					 struct super_block *sb,
					 unsigned int group,
					 struct buffer_head *bitmap_bh )
{
	struct ext2_reserve_window_node	*search_head;
	unsigned long					group_first_block;
	unsigned long					group_end_block;
	unsigned long					start_block;
	unsigned long					first_free_block;
	struct rb_root					*fs_rsv_root;
	unsigned long					size;
	int								ret;
	spinlock_t						*rsv_lock;

	fs_rsv_root			= &ME2FS_SB( sb )->s_rsv_window_root;
	rsv_lock			= &ME2FS_SB( sb )->s_rsv_window_lock;

	group_first_block	= ext2GetFirstBlockNum( sb, group );
	group_end_block		= group_first_block +
						  ( ME2FS_SB( sb )->s_blocks_per_group - 1 );

	if( grp_goal < 0 )
	{
		start_block = group_first_block;
	}
	else
	{
		start_block = grp_goal + group_first_block;
	}

	size = my_rsv->rsv_goal_size;

	if( !isRsvEmpty( &my_rsv->rsv_window ) )
	{
		/* -------------------------------------------------------------------- */
		/* if the old reservation is cross group boundary and if the goal is 	*/
		/* inside the old reservation window, we will come here when we just	*/
		/* failed to allocate from the first part of the window. we still have	*/
		/* another part that belongs to the next group. in this case, there is	*/
		/* no point to discard our window and try to allocate a new one in this	*/
		/* group (which will fail). we should keep the reservation window,		*/
		/* just simply move on.													*/
		/*																		*/
		/* maybe we could shift the start block of the reservation window to	*/
		/* the first block of next group.										*/
		/* -------------------------------------------------------------------- */
		if( ( my_rsv->rsv_start	<= group_end_block ) &&
			( group_end_block	<  my_rsv->rsv_end   ) &&
			( my_rsv->rsv_start <= start_block       ) )
		{
			return( -1 );
		}

		if( ( ( my_rsv->rsv_end - my_rsv->rsv_start + 1 ) / 2 )
			< my_rsv->rsv_alloc_hit )
		{
			/* ---------------------------------------------------------------- */
			/* if the previously allocation hit ratio is greater than 1/2,		*/
			/* then we double the size of the reservation window the next time,	*/
			/* otherwise we keep the same size window							*/
			/* ---------------------------------------------------------------- */
			size = size * 2;
			if( EXT2_MAX_RESERVE_BLOCKS < size )
			{
				size = EXT2_MAX_RESERVE_BLOCKS;
			}

			my_rsv->rsv_goal_size = size;
		}
	}

	spin_lock( rsv_lock );
	{
		/* -------------------------------------------------------------------- */
		/* shift the search start to the window near the goal block				*/
		/* -------------------------------------------------------------------- */
		search_head = searchReserveWindow( fs_rsv_root, start_block );

		/* -------------------------------------------------------------------- */
		/* findNextReservableWindow() simply finds a reservable window inside	*/
		/* the given range (start_block, group_end_block).						*/
		/* 																		*/
		/* To make sure the reservation window has a free bit inside it, we		*/
		/* need to check the bitmap after we found a reservable window.			*/
		/* -------------------------------------------------------------------- */
retry:
		ret = findNextReservableWindow( search_head,
										my_rsv,
										sb,
										start_block,
										group_end_block );

		if( ret == -1 )
		{
			if( !isRsvEmpty( &my_rsv->rsv_window ) )
			{
				removeReserveWindow( sb, my_rsv );
			}
			spin_unlock( rsv_lock );
			return( -1 );
		}
	}
	spin_unlock( rsv_lock );
	/* ------------------------------------------------------------------------ */
	/* on success, findNextReservableWindow() returns the reservation			*/
	/* window where there is a reservable space after it. before we reserve		*/
	/* this reservable space, we need to make sure there is at least a free		*/
	/* block inside this region.												*/
	/*																			*/
	/* search the first free bit on the block bitmpa. search starts from		*/
	/* the start block of the reservable space we just found.					*/
	/* ------------------------------------------------------------------------ */
	first_free_block =
		searchBitmapNextUsableBlock( my_rsv->rsv_start - group_first_block,
									 bitmap_bh,
									 group_end_block - group_first_block + 1 );
	
	if( first_free_block < 0 )
	{
		/* -------------------------------------------------------------------- */
		/* no free block left on the bitmap, no point to reserve the space		*/
		/* -------------------------------------------------------------------- */
		/* return failed														*/
		spin_lock( rsv_lock );
		{
			if( !isRsvEmpty( &my_rsv->rsv_window ) )
			{
				removeReserveWindow( sb, my_rsv );
			}
		}
		spin_unlock( rsv_lock );
		return( -1 );
	}

	start_block = first_free_block + group_first_block;

	/* ------------------------------------------------------------------------ */
	/* check if the first free block is within the free space we just reserved	*/
	/* ------------------------------------------------------------------------ */
	if( ( my_rsv->rsv_start <= start_block ) &&
		( start_block <= my_rsv->rsv_end ) )
	{
		/* success																*/
		return( 0 );
	}

	/* ------------------------------------------------------------------------ */
	/* if the first free bit we found is out of the reservable space, continue	*/
	/* search for next reservable spze, start from where the free block is,		*/
	/* we also shift the list head to where we stopped last time				*/
	/* ------------------------------------------------------------------------ */
	search_head = my_rsv;
	spin_lock( rsv_lock );
	goto retry;
}
/*
==================================================================================
	Function	:searchReserveWindow
	Input		:struct rb_root *root
				 < root of reservation tree >
				 unsigned long goal
				 < target allocation blokc >
	Output		:void
	Return		:ext2_reserve_window_node*
				 < found reservation >

	Description	:find the reserved window which includes the goal, or previous one
==================================================================================
*/
static struct ext2_reserve_window_node*
searchReserveWindow( struct rb_root *root, unsigned long goal )
{
	struct rb_node					*node;
	struct ext2_reserve_window_node	*rsv;

	if( !( node = root->rb_node ) )
	{
		return( NULL );
	}

	do
	{
		rsv = rb_entry( node, struct ext2_reserve_window_node, rsv_node );

		if( goal < rsv->rsv_start )
		{
			node = node->rb_left;
		}
		else if( rsv->rsv_end < goal )
		{
			node = node->rb_right;
		}
		else
		{
			return( rsv );
		}
	}
	while( node );

	/* ------------------------------------------------------------------------ */
	/* we've fallen off the end of the tree:the goal was'nt inside any			*/
	/* particular node. the previous node must be to one side of the interval	*/
	/* containing the goal. if it's the rhs, we need to back up one.			*/
	/* ------------------------------------------------------------------------ */
	if( goal < rsv->rsv_start )
	{
		node	= rb_prev( &rsv->rsv_node );
		rsv		= rb_entry( node, struct ext2_reserve_window_node, rsv_node );
	}

	return( rsv );
}
/*
==================================================================================
	Function	:removeReserveWindow
	Input		:struct super_block *sb
				 < vfs super block >
				 struct ext2_reserve_window_node *rsv
				 < reservation window >
	Output		:void
	Return		:void

	Description	:unlink a window from the reservation rb tree
==================================================================================
*/
static void removeReserveWindow( struct super_block *sb,
								 struct ext2_reserve_window_node *rsv )
{
	rsv->rsv_start		= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
	rsv->rsv_end		= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;

	rsv->rsv_alloc_hit	= 0;

	rb_erase( &rsv->rsv_node, &ME2FS_SB( sb )->s_rsv_window_root );
}
/*
==================================================================================
	Function	:searchBitmapNextUsableBlock
	Input		:unsigned long start
				 < start block >
				 struct buffer_head *bh
				 < buffer cache contains the block group bitmap >
				 unsigned long end
				 < end block >
	Output		:void
	Return		:unsigned long
				 < free block number >

	Description	:search forward through the actual bitmap on disk unitl finding
				 a free bit
==================================================================================
*/
static unsigned long
searchBitmapNextUsableBlock( unsigned long start,
							 struct buffer_head *bh,
							 unsigned long end )
{
	unsigned long	next;

	next = find_next_zero_bit_le( bh->b_data, end, start );

	if( end <= next )
	{
		return( -1 );
	}

	return( next );
}

/*
==================================================================================
	Function	:tyrToExtendReservation
	Input		:struct ext2_reserve_sindow_node *my_rsv
				 < the window reservation >
				 struct super_block *sb
				 < vfs super block >
				 int size
				 < size to extend >
	Output		:void
	Return		:void

	Description	:attempt to expand the reservation window large enough to have
				 required number of free blocks
==================================================================================
*/
static void
tryToExtendReservation( struct ext2_reserve_window_node *my_rsv,
						struct super_block *sb,
						int size )
{
	struct ext2_reserve_window_node	*next_rsv;
	struct rb_node					*next_rb;
	spinlock_t						*rsv_lock;
	int								ret;

	rsv_lock = &ME2FS_SB( sb )->s_rsv_window_lock;

	ret = spin_trylock( rsv_lock );
	{
		if( !ret )
		{
			return;
		}

		if( !( next_rb = rb_next( &my_rsv->rsv_node ) ) )
		{
			my_rsv->rsv_end += size;
		}
		else
		{
			next_rsv = rb_entry( next_rb,
								 struct ext2_reserve_window_node,
								 rsv_node );

			if( size <= ( next_rsv->rsv_start - my_rsv->rsv_end - 1 ) )
			{
				my_rsv->rsv_end += size;
			}
			else
			{
				my_rsv->rsv_end = next_rsv->rsv_start - 1;
			}
		}
	}
	spin_unlock( rsv_lock );
}
/*
==================================================================================
	Function	:findNextReservableWindow
	Input		:struct ext2_reserve_window_node *search_head
				 < the head of the searching list >
				 struct ext2_reserve_window_node *my_rsv
				 < reservation window >
				 struct super_block *sb
				 < vfs super block >
				 unsigned long start_block
				 < start block to start search >
				 unsigned long last_block
				 < last block to end search >
	Output		:void
	Return		:int
				 < result >

	Description	:find a reservable space within the given range.
==================================================================================
*/
static int
findNextReservableWindow( struct ext2_reserve_window_node *search_head,
						  struct ext2_reserve_window_node *my_rsv,
						  struct super_block *sb,
						  unsigned long start_block,
						  unsigned long last_block )
{
	struct rb_node					*next;
	struct ext2_reserve_window_node	*rsv;
	struct ext2_reserve_window_node	*prev;
	unsigned long					cur;
	int								size;

	size	= my_rsv->rsv_goal_size;
	cur		= start_block;

	if( !( rsv = search_head ) )
	{
		return( -1 );
	}

	while( 1 )
	{
		if( cur <= rsv->rsv_end )
		{
			cur = rsv->rsv_end + 1;
		}

		if( last_block < cur )
		{
			/* failed															*/
			return( -1 );
		}

		prev	= rsv;
		next	= rb_next( &rsv->rsv_node );
		rsv		= rb_entry( next, struct ext2_reserve_window_node, rsv_node );

		/* -------------------------------------------------------------------- */
		/* reached tha last reservation, we can just append to the previous one	*/
		/* -------------------------------------------------------------------- */
		if( !next )
		{
			break;
		}

		if( ( cur + size ) <= rsv->rsv_start )
		{
			/* ---------------------------------------------------------------- */
			/* found a reservable space big enough. we could have a reservation	*/
			/* across the group boundary here									*/
			/* ---------------------------------------------------------------- */
			break;
		}
	}
	/* ------------------------------------------------------------------------ */
	/* we come here either:														*/
	/* when we reach the end of the whole list, and there is empty reservable	*/
	/* space after last entry in the list. append it to the end of the list.	*/
	/* or we found one reservable space in the middle of the list, return the	*/
	/* reservation window that we could append to.suceed						*/
	/* ------------------------------------------------------------------------ */
	if( ( prev != my_rsv ) &&
		( !isRsvEmpty( &my_rsv->rsv_window ) ) )
	{
		removeReserveWindow( sb, my_rsv );
	}
	/* ------------------------------------------------------------------------ */
	/* let's book the whole available window for now. we will check the disk	*/
	/* bitmap later and then, if there are free blocks then we adjust the		*/
	/* window size if it's larger than requested. otherwise, we will remove		*/
	/* this node from the tree next time call findNextReservalbeWindow()		*/
	/* ------------------------------------------------------------------------ */
	my_rsv->rsv_start		= cur;
	my_rsv->rsv_end			= cur + size - 1;
	my_rsv->rsv_alloc_hit	= 0;

	if( prev != my_rsv )
	{
		me2fsInsertReserveWindow( sb, my_rsv );
	}

	return( 0 );
}

/*
==================================================================================
	Function	:findNextUsableBlock
	Input		:int start
				 < start block >
				 struct buffer_head *bh
				 < buffer cache contains the block group bitmap >
				 int end
				 < end block >
	Output		:void
	Return		:unsigned long
				 < found block number >

	Description	:find an allocatable block in a bitmap.
==================================================================================
*/
static unsigned long
findNextUsableBlock( int start, struct buffer_head *bh, int end )
{
	unsigned long	here;
	unsigned long	next;
	char			*p;
	char			*r;

	if( 0 < start )
	{
		/* -------------------------------------------------------------------- */
		/* the goal was occupied; search forward for a free block within the	*/
		/* next xx blocks.														*/
		/* end_goal is more or less random, but it has to be less than			*/
		/* s_blocks_per_group. aligning up to the next 64-bit boundary is simple*/
		/* -------------------------------------------------------------------- */
		unsigned long	end_goal;

		end_goal = ( start + 63 ) & ~63;

		if( end < end_goal )
		{
			end_goal = end;
		}

		here = find_next_zero_bit_le( bh->b_data, end_goal, start );

		if( here < end_goal )
		{
			return( here );
		}
		DBGPRINT( "<ME2FS>%s:bit not found near goal\n", __func__ );
	}

	here = start;

	if( here < 0 )
	{
		here = 0;
	}

	p		= ( ( char* )bh->b_data ) + ( here >> 3 );
	r		= memscan( p, 0, ( ( end + 7 ) >> 3 ) - ( here >> 3 ) );
	next	= ( r - ( ( char* )bh->b_data ) ) << 3;

	if( ( next < end ) && ( here <= next ) )
	{
		return( next );
	}

	return( searchBitmapNextUsableBlock( here, bh, end ) );
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
