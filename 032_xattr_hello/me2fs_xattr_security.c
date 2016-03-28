/********************************************************************************
	File			: me2fs_xattr_user.c
	Description		: Operations for User Extended Attribute Handler

*********************************************************************************/
#include <linux/xattr.h>
#include <linux/security.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_xattr.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static size_t listSecurityXattr( struct dentry *dentyr,
								 char *list,
								 size_t list_size,
								 const char *name,
								 size_t name_len,
								 int type );
static int getSecurityXattr( struct dentry *dentry,
							 const char *name,
							 void *buffer,
							 size_t size,
							 int type );
static int setSecurityXattr( struct dentry *dentry,
							 const char *name,
							 const void *value,
							 size_t size,
							 int flags,
							 int type );

static int initXattrs( struct inode *inode,
					   const struct xattr *xattr_array,
					   void *fs_info );

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
const struct xattr_handler me2fs_xattr_security_handler = {
	.prefix	= XATTR_SECURITY_PREFIX,
	.list	= listSecurityXattr,
	.get	= getSecurityXattr,
	.set	= setSecurityXattr,
};

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitSecurity
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *dir
				 < vfs inode of parent directory >
				 const struct qstr *qstr
				 < name >
	Output		:void
	Return		:void

	Description	:initialize security
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitSecurity( struct inode *inode,
					   struct inode *dir,
					   const struct qstr *qstr )
{
	return( security_inode_init_security( inode, dir, qstr, initXattrs, NULL ) );
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
	Function	:listSecurityXattr
	Input		:struct dentry *dentry
				 < file entry >
				 char *list
				 < user buffer via kernel buffer >
				 size_t list_size
				 < size of buffer >
				 const char *name
				 < name of xattr >
				 size_t name_len
				 < length of name >
				 int type
				 < type of handler >
	Output		:void
	Return		:size_t
				 < size of xattr list >

	Description	:list security xattr
==================================================================================
*/
static size_t listSecurityXattr( struct dentry *dentry,
								 char *list,
								 size_t list_size,
								 const char *name,
								 size_t name_len,
								 int type )
{
	size_t total_len;

	total_len = XATTR_SECURITY_PREFIX_LEN + name_len + 1;

	if( list && total_len <= list_size )
	{
		memcpy( list, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN );
		memcpy( list + XATTR_SECURITY_PREFIX_LEN, name, name_len );
		list[ XATTR_SECURITY_PREFIX_LEN + name_len ] = '\0';
	}

	return( total_len );
}

/*
==================================================================================
	Function	:getSecurityXattr
	Input		:struct dentry *dentry
				 < file entry >
				 const char *name
				 < name of xattr >
				 void *buffer
				 < user buffer via kernel buffer >
				 size_t size
				 < size of xattr >
				 int type
				 < type of handler >
	Output		:void
	Return		:int
				 < result >

	Description	:get security xattr
==================================================================================
*/
static int getSecurityXattr( struct dentry *dentry,
							 const char *name,
							 void *buffer,
							 size_t size,
							 int type )
{
	if( strcmp( name, "" ) == 0 )
	{
		return( -EINVAL );
	}

	return( me2fsGetXattr( dentry->d_inode,
						   EXT2_XATTR_INDEX_SECURITY,
						   name,
						   buffer,
						   size ) );
}
/*
==================================================================================
	Function	:setSecurityXattr
	Input		:struct dentry *dentry
				 < file entry >
				 const char *name
				 < name of xattr >
				 const void *value
				 < value of xattr >
				 size_t size
				 < size of xattr >
				 int flags
				 < flags of xattr >
				 int type
				 < type of handler >
	Output		:void
	Return		:int
				 < resutl >

	Description	:set security xattr to a file
==================================================================================
*/
static int setSecurityXattr( struct dentry *dentry,
							 const char *name,
							 const void *value,
							 size_t size,
							 int flags,
							 int type )
{
	if( strcmp( name, "" ) == 0 )
	{
		return( -EINVAL );
	}

	return( me2fsSetXattr( dentry->d_inode,
						   EXT2_XATTR_INDEX_SECURITY,
						   name,
						   value,
						   size,
						   flags ) );
}
/*
==================================================================================
	Function	:initXattrs
	Input		:struct inode *inode
				 < vfs inode >
				 const struct xattr *xattr_array
				 < array of initial xattrs >
				 void *fs_info
				 < private information >
	Output		:void
	Return		:int
				 < result >

	Description	:initialize xattrs
==================================================================================
*/
static int initXattrs( struct inode *inode,
					   const struct xattr *xattr_array,
					   void *fs_info )
{
	const struct xattr	*xattr;
	int					err;

	err = 0;

	for( xattr = xattr_array ; xattr->name != NULL ; xattr++ )
	{
		err = me2fsSetXattr( inode,
							 EXT2_XATTR_INDEX_SECURITY,
							 xattr->name,
							 xattr->value,
							 xattr->value_len,
							 0 );

		if( err < 0 )
		{
			break;
		}
	}

	return( err );
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
