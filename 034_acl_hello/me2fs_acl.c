/********************************************************************************
	File			: me2fs_acl.c
	Description		: Operations for User Extended Attribute Handler

*********************************************************************************/
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/posix_acl_xattr.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_xattr.h"


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
	Function	:me2fsGetAcl
	Input		:struct inode *inode
				 < vfs inode >
				 int type
				 < type of acl >
	Output		:void
	Return		:struct posix_acl*
				 < return posix acl >

	Description	:get posix acl
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct posix_acl*
me2fsGetAcl( struct inode *inode, int type )
{
	int					name_index;
	struct posix_acl	*acl;
	uid_t				user_id;

	DBGPRINT( "<ME2FS>%s:get acl[ino=%lu, type=%d]\n",
			  __func__, inode->i_ino, type );

	switch( type )
	{
	case	ACL_TYPE_ACCESS:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_ACCESS;
		break;
	case	ACL_TYPE_DEFAULT:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT;
		break;
	default:
		DBGPRINT( "<ME2FS>%s:bug\n", __func__ );
		return( NULL );
	}

	acl = posix_acl_alloc( 4, GFP_KERNEL );

	if( !acl )
	{
		return( ERR_PTR( -ENOMEM ) );
	}

	acl->a_count = 4;

	user_id = 1000;

	acl->a_entries[ 0 ].e_tag	= ACL_USER_OBJ;
	acl->a_entries[ 0 ].e_perm	= S_IRWXU >> 6;
	acl->a_entries[ 0 ].e_uid	= make_kuid( &init_user_ns, user_id );
	acl->a_entries[ 0 ].e_gid	= make_kgid( &init_user_ns, user_id );

	acl->a_entries[ 1 ].e_tag	= ACL_GROUP_OBJ;
	acl->a_entries[ 1 ].e_perm	= S_IRWXG >> 3;
	acl->a_entries[ 1 ].e_uid	= make_kuid( &init_user_ns, user_id );
	acl->a_entries[ 1 ].e_gid	= make_kgid( &init_user_ns, user_id );

	acl->a_entries[ 2 ].e_tag	= ACL_OTHER;
	acl->a_entries[ 2 ].e_perm	= S_IRWXO;
	acl->a_entries[ 2 ].e_uid	= make_kuid( &init_user_ns, user_id );
	acl->a_entries[ 2 ].e_gid	= make_kgid( &init_user_ns, user_id );

	acl->a_entries[ 3 ].e_tag	= ACL_USER;
	acl->a_entries[ 3 ].e_perm	= S_IRWXU >> 6;
	acl->a_entries[ 3 ].e_uid	= make_kuid( &init_user_ns, user_id );
	acl->a_entries[ 3 ].e_gid	= make_kgid( &init_user_ns, user_id );

	set_cached_acl( inode, type, acl );

	return( acl );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetAcl
	Input		:struct inode *inode
				 < vfs inode >
				 struct posix_acl *acl
				 < posix acl >
				 int type
				 < acl type >
	Output		:void
	Return		:int
				 < result >

	Description	:set posix acl
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsSetAcl( struct inode *inode, struct posix_acl *acl, int type )
{
	int	name_index;
	int	i;

	DBGPRINT( "<ME2FS>%s:get acl[ino=%lu, type=%d]\n",
			  __func__, inode->i_ino, type );

	switch( type )
	{
	case	ACL_TYPE_ACCESS:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_ACCESS;
		break;
	case	ACL_TYPE_DEFAULT:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT;
		break;
	default:
		return( -EINVAL );
	}

	for( i = 0 ; i < acl->a_count ; i++ )
	{
		DBGPRINT( "<ME2FS>[%d]e_tag:%d e_perm:%u e_uid:%u e_gid:%u\n",
				  i,
				  acl->a_entries[ i ].e_tag,
				  acl->a_entries[ i ].e_perm,
				  acl->a_entries[ i ].e_uid.val,
				  acl->a_entries[ i ].e_gid.val );
	}

	return( 0 );
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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
