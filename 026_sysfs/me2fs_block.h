/*********************************************************************************
	File			: me2fs_block.h
	Description		: Definitions for block group operations

*********************************************************************************/
#ifndef	__ME2FS_BLOCK_H__
#define	__ME2FS_BLOCK_H__

#include "me2fs.h"

/*
==================================================================================

	Prototype Statement

==================================================================================
*/

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
	Function	:me2fsGetGroupDescriptor
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int block_group
				 < block group number >
	Output		:void
	Return		:struct ext2_group_desc*
				 < goup descriptor >

	Description	:get group descriptor
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct ext2_group_desc*
me2fsGetGroupDescriptor( struct super_block *sb,
						unsigned int block_group );

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
me2fsGetGdescBufferCache( struct super_block *sb, unsigned int block_group );

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
int me2fsHasBgSuper( struct super_block *sb, int group );

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
unsigned long me2fsCountFreeBlocks( struct super_block *sb );

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
				int*err );

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
					  unsigned long count );

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
me2fsGetBlocksUsedByGroupTable( struct super_block *sb, int group );

#endif	// __ME2FS_BLOCK_H__
