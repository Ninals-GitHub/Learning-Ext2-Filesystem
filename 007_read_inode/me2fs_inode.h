/*********************************************************************************
	File			: me2fs_inode.h
	Description		: Definition for inode of my ext2 file system

*********************************************************************************/
#ifndef	__ME2FS_INODE_H__
#define	__ME2FS_INODE_H__


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
struct inode *me2fsGetVfsInode( struct super_block *sb, unsigned int ino );

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
void me2fsSetVfsInodeFlags( struct inode *inode );

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
void dbgPrintVfsInode( struct inode *inode );

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
void dbgPrintMe2fsInodeInfo( struct me2fs_inode_info *mei );

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
void dbgPrintExt2InodeInfo( struct ext2_inode *ei );

#endif	// __ME2FS_INODE_H
