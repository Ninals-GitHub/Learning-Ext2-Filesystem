/********************************************************************************
	File			: me2fs_symlink.c
	Description		: symbolic link operations for my ext2 file system

*********************************************************************************/
#include "linux/namei.h"

#include "me2fs.h"
#include "me2fs_util.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static void*
me2fsFollowLink( struct dentry *dentry, struct nameidata *nd );

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
const struct inode_operations me2fs_symlink_inode_operations =
{
	.readlink		= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link		= page_put_link,
	//.setattr		= me2fsSetAttr,
};

const struct inode_operations me2fs_fast_symlink_inode_operations =
{
	.readlink		= generic_readlink,
	.follow_link	= me2fsFollowLink,
	//.setattr		= me2fsSetAttr,
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
	Function	:me2fsFollowLink
	Input		:struct dentry *dentry
				 < symlink file >
				 struct nameidata *nd
				 < data of name resolver >
	Output		:void
	Return		:void*
				 < return address except for PTR_ERR >

	Description	:follo symbolic link
==================================================================================
*/
static void*
me2fsFollowLink( struct dentry *dentry, struct nameidata *nd )
{
	struct me2fs_inode_info	*mi;

	mi = ME2FS_I( dentry->d_inode );

	nd_set_link( nd, ( char* )mi->i_data );
	return( NULL );
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
