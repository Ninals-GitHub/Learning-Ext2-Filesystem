/*********************************************************************************
	File			: me2fs_xattr.h
	Description		: Definition for Extended Attribute

*********************************************************************************/
#ifndef	__ME2FS_XATTR_H__
#define	__ME2FS_XATTR_H__


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
extern const struct xattr_handler *me2fs_xattr_handlers[ ];

/*
==================================================================================

	DEFINES

==================================================================================
*/
/*
----------------------------------------------------------------------------------
	Name indexes
----------------------------------------------------------------------------------
*/
#define	EXT2_XATTR_INDEX_USER					1
#define	EXT2_XATTR_INDEX_POSIX_ACL_ACCESS		2
#define	EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT		3
#define	EXT2_XATTR_INDEX_TRUSTED				4
#define	EXT2_XATTR_INDEX_LUSTRE					5
#define	EXT2_XATTR_INDEX_SECURITY				6

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
	Function	:me2fsGetXattr
	Input		:struct inode *inode
				 < vfs inode >
				 int name_index
				 < name index of xattr >
				 const char *name
				 < name of xattr >
				 void *buffer
				 < user buffer via kernel buffer >
				 size_t buffer_size
				 < size of buffer to read >
	Output		:void
	Return		:int
				 < result >

	Description	:get xattr
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsGetXattr( struct inode *inode,
				   int name_index,
				   const char *name,
				   void *buffer,
				   size_t buffer_size );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetXattr
	Input		:struct inode *inode
				 < vfs inode >
				 int name_index
				 < name index of xattr >
				 const char *name
				 < name of xattr >
				 const void *value
				 < buffer of xattr value >
				 size_t value_len
				 < size of buffer to write >
				 int flags
				 < flags of xattr >
	Output		:void
	Return		:int
				 < result >

	Description	:set xattr
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsSetXattr( struct inode *inode,
				   int name_index,
				   const char *name,
				   const void *value,
				   size_t value_len,
				   int flags );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsListXattr
	Input		:struct dentry *dentry
				 < vfs dentry >
				 char *buffer
				 < user buffer via kernel buffer >
				 size_t buffer_size
				 < size of buffer >
	Output		:void
	Return		:ssize_t
				 < result >

	Description	:list xattrs
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
ssize_t me2fsListXattr( struct dentry *dentry,
						char *buffer,
						size_t buffer_size );

#endif	// __ME2FS_XATTR_H__
