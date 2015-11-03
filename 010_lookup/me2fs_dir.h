/*********************************************************************************
	File			: me2fs_dir.h
	Description		: Definitions for Directory Operations

*********************************************************************************/
#ifndef	__ME2FS_DIR_H__
#define	__ME2FS_DIR_H__


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
extern const struct file_operations me2fs_dir_operations;

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
	Function	:me2fsGetInoByName
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct qstr *child
				 < query name which is child of the directory >
	Output		:void
	Return		:ino_t
				 < inode number >

	Description	:lookup by name in directory and return its inode number
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
ino_t me2fsGetInoByName( struct inode *dir, struct qstr *child );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsFindDirEntry
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct qstr *child
				 < query;child of directory >
				 struct page **res_page
				 < output >
	Output		:struct page **res_page
				 < the entry was found in >
	Return		:struct ext2_dir_entry *
				 < found entry >

	Description	:find an entry in the directory, ouput the page in which the
				 entry was found( **res_page ) and return entry.
				 page is returned mapped and unlocked. the entry is guaranteed
				 to be valid.
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct ext2_dir_entry*
me2fsFindDirEntry( struct inode *dir,
				   struct qstr *child,
				   struct page **res_page );

#endif	// __ME2FS_DIR_H__
