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

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsMakeEmpty
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *parent
				 < parent inode >
	Output		:void
	Return		:int
				 < result >

	Description	:make empty entry
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsMakeEmpty( struct inode *inode, struct inode *parent );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsAddLink
	Input		:struct dentry *dentry
				 < vfs dentry >
				 struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:add link
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsAddLink( struct dentry *dentry, struct inode *inode );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsIsEmptyDir
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:int
				 < 0:false 1:true >

	Description	:check if the directory is empty or not
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsIsEmptyDir( struct inode *inode );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsDeleteDirEntry
	Input		:struct ext2_dir_entry *dir
				 < directory entry to delete >
				 struct page *page
				 < page cache the directory belongs to >
	Output		:void
	Return		:void

	Description	:delete a directory entry
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsDeleteDirEntry( struct ext2_dir_entry *dir, struct page *page );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetDotDotEntry
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct page **p
				 < page to which dot dot entry belongs to >
	Output		:struct page **p
				 < page to which dot dot entry belongs to >
	Return		:struct ext2_dir_entry*
				 < dot dot entry >

	Description	:get dot dot entry
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct ext2_dir_entry*
me2fsGetDotDotEntry( struct inode *dir, struct page **p );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetLink
	Input		:struct inode *dir
				 < vfs inode >
				 struct ext2_dir_entry *dent
				 < directory entry to set link >
				 struct page *page
				 < directory page cache >
				 struct inode *inode
				 < vfs inode to be linked >
				 int update_times
				 < flag to update times >
	Output		:void
	Return		:void

	Description	:set link
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsSetLink( struct inode *dir,
				   struct ext2_dir_entry *dent,
				   struct page *page,
				   struct inode *inode,
				   int update_times );

#endif	// __ME2FS_DIR_H__
