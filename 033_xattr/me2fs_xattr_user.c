/********************************************************************************
	File			: me2fs_xattr_user.c
	Description		: Operations for User Extended Attribute Handler

*********************************************************************************/
#include <linux/xattr.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_xattr.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static size_t listUserXattr( struct dentry *dentyr,
							 char *list,
							 size_t list_size,
							 const char *name,
							 size_t name_len,
							 int type );
static int getUserXattr( struct dentry *dentry,
						 const char *name,
						 void *buffer,
						 size_t size,
						 int type );
static int setUserXattr( struct dentry *dentry,
						  const char *name,
						  const void *value,
						  size_t size,
						  int flags,
						  int type );

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
const struct xattr_handler me2fs_xattr_user_handler = {
	.prefix	= XATTR_USER_PREFIX,
	.list	= listUserXattr,
	.get	= getUserXattr,
	.set	= setUserXattr,
};

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
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
	Function	:listUserXattr
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

	Description	:list user xattr
==================================================================================
*/
static size_t listUserXattr( struct dentry *dentry,
							 char *list,
							 size_t list_size,
							 const char *name,
							 size_t name_len,
							 int type )
{
	size_t total_len;

	DBGPRINT( "<me2fs>%s:\n", __func__ );

	total_len = XATTR_USER_PREFIX_LEN + name_len + 1;

	if( !( ME2FS_SB( dentry->d_sb )->s_mount_opt & EXT2_MOUNT_XATTR_USER ) )
	{
		return( 0 );
	}

	if( list && total_len <= list_size )
	{
		memcpy( list, XATTR_USER_PREFIX, XATTR_USER_PREFIX_LEN );
		memcpy( list + XATTR_USER_PREFIX_LEN, name, name_len );
		list[ XATTR_USER_PREFIX_LEN + name_len ] = '\0';
	}

	return( total_len );
}

/*
==================================================================================
	Function	:getUserXattr
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

	Description	:get user xattr
==================================================================================
*/
static int getUserXattr( struct dentry *dentry,
						 const char *name,
						 void *buffer,
						 size_t size,
						 int type )
{
	DBGPRINT( "<me2fs>%s:\n", __func__ );

	if( strcmp( name, "" ) == 0 )
	{
		return( -EINVAL );
	}

	if( !( ME2FS_SB( dentry->d_sb )->s_mount_opt & EXT2_MOUNT_XATTR_USER ) )
	{
		return( -EOPNOTSUPP );
	}

	return( me2fsGetXattr( dentry->d_inode,
						   EXT2_XATTR_INDEX_USER,
						   name,
						   buffer,
						   size ) );
}
/*
==================================================================================
	Function	:setUserXattr
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

	Description	:set user xattr to a file
==================================================================================
*/
static int setUserXattr( struct dentry *dentry,
						  const char *name,
						  const void *value,
						  size_t size,
						  int flags,
						  int type )
{
	DBGPRINT( "<me2fs>%s:\n", __func__ );

	if( strcmp( name, "" ) == 0 )
	{
		return( -EINVAL );
	}

	if( !( ME2FS_SB( dentry->d_sb )->s_mount_opt & EXT2_MOUNT_XATTR_USER ) )
	{
		return( -EOPNOTSUPP );
	}

	return( me2fsSetXattr( dentry->d_inode,
						   EXT2_XATTR_INDEX_USER,
						   name,
						   value,
						   size,
						   flags ) );
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
