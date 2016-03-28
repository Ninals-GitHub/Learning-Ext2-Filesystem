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
	Magic value in attribute blocks
----------------------------------------------------------------------------------
*/
#define	EXT2_XATTR_MAGIC			0xEA020000

/*
----------------------------------------------------------------------------------
	Maximum number of references to one attribute block
----------------------------------------------------------------------------------
*/
#define	EXT2_XATTR_REFCOUNT_MAX		1024

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
----------------------------------------------------------------------------------
	Xattr header
----------------------------------------------------------------------------------
*/
struct ext2_xattr_header
{
	__le32	h_magic;				/* magic number for identification			*/
	__le32	h_refcount;				/* reference count							*/
	__le32	h_blocks;				/* number of disk blocks used				*/
	__le32	h_hash;					/* hash value of all attributes				*/
	__u32	h_reserved[ 4 ];		/* zero right now							*/
};

/*
----------------------------------------------------------------------------------
	Xattr entry
----------------------------------------------------------------------------------
*/
struct ext2_xattr_entry
{
	__u8	e_name_len;				/* length of name							*/
	__u8	e_name_index;			/* attribute name index						*/
	__le16	e_value_offs;			/* offset in disk block of value			*/
	__le32	e_value_block;			/* disk block attribure is stored on (n/i)	*/
	__le32	e_value_size;			/* size of attribute value					*/
	__le32	e_hash;					/* hash value of name and value				*/
	char	e_name[ 0 ];			/* attribute name							*/
};

#define	EXT2_XATTR_PAD_BITS			2
#define	EXT2_XATTR_PAD				( 1 << EXT2_XATTR_PAD_BITS )
#define	EXT2_XATTR_ROUND			( EXT2_XATTR_PAD - 1 )

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

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsDeleteXattr
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:delete xattr. this function is called immediately before
				 the associated inode is freed
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsDeleteXattr( struct inode *inode );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsXattrPutSuper
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:shrink mb cache when put super
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsXattrPutSuper( struct super_block *sb );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitXattr
	Input		:void
	Output		:void
	Return		:int
				 < result >

	Description	:initialize xattr
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitXattr( void );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsExitXattr
	Input		:void
	Output		:void
	Return		:void

	Description	:destry xattr mbcache
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsExitXattr( void );


#endif	// __ME2FS_XATTR_H__
