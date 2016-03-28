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
#include "me2fs_acl.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static inline size_t getAclSize( int count );
static inline int getAclCount( size_t size );
static struct posix_acl* aclFromDisk( const void* value, size_t size );
static void* aclToDisk( const struct posix_acl *acl, size_t *size );

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
	char				*value;
	int					retval;

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

	value	= NULL;

	/* ------------------------------------------------------------------------ */
	/* get size of acl entries													*/
	/* ------------------------------------------------------------------------ */
	retval	= me2fsGetXattr( inode, name_index, "", NULL, 0 );

	if( 0 < retval )
	{
		value = kmalloc( retval, GFP_KERNEL );

		if( !value )
		{
			return( ERR_PTR( -ENOMEM ) );
		}

		retval = me2fsGetXattr( inode, name_index, "", value, retval );
	}

	if( 0 < retval )
	{
		acl = aclFromDisk( value, retval );
	}
	else if( ( retval == -ENODATA ) || ( retval == -ENOSYS ) )
	{
		DBGPRINT( "<ME2FS>%s:acl is null\n", __func__ );
		if( retval == -ENODATA )
		{
			DBGPRINT( "<ME2FS>%s:enodata\n", __func__ );
		}
		else
		{
			DBGPRINT( "<ME2FS>%s:enosys\n", __func__ );
		}
		acl = NULL;
	}
	else
	{
		acl = ERR_PTR( retval );
	}

	kfree( value );

	if( !IS_ERR( acl ) )
	{
		set_cached_acl( inode, type, acl );
	}

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
	int		name_index;
	int		error;
	void	*value;
	size_t	size;

	DBGPRINT( "<ME2FS>%s:get acl[ino=%lu, type=%d]\n",
			  __func__, inode->i_ino, type );

	switch( type )
	{
	case	ACL_TYPE_ACCESS:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_ACCESS;
		if( acl )
		{
			error = posix_acl_equiv_mode( acl, &inode->i_mode );

			if( error < 0 )
			{
				return( error );
			}
			else
			{
				inode->i_ctime = CURRENT_TIME_SEC;
				mark_inode_dirty( inode );
				if( error == 0 )
				{
					acl = NULL;
				}
			}
		}
		break;
	case	ACL_TYPE_DEFAULT:
		name_index = EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT;
		if( !S_ISDIR( inode->i_mode ) )
		{
			if( acl )
			{
				return( -EACCES );
			}
			else
			{
				return( 0 );
			}
		}
		break;
	default:
		return( -EINVAL );
	}

	value	= NULL;
	size	= 0;

	if( acl )
	{
		value = aclToDisk( acl, &size );
		if( IS_ERR( value ) )
		{
			return( ( int )PTR_ERR( value ) );
		}
	}

	error = me2fsSetXattr( inode, name_index, "", value, size, 0 );

	kfree( value );
	if( !error )
	{
		set_cached_acl( inode, type, acl );
	}

	return( error );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitAcl
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *dir
				 < vfs inode of parent directory >
	Output		:void
	Return		:int
				 < result >

	Description	:initialize the acls of a new inode.
				 called from me2fsAllocNewInode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitAcl( struct inode *inode, struct inode *dir )
{
	struct posix_acl	*acl;
	struct posix_acl	*default_acl;
	int					error;

	acl		= NULL;

	error = posix_acl_create( dir, &inode->i_mode, &default_acl, &acl);

	if( error )
	{
		return( error );
	}

	if( default_acl )
	{
		error = me2fsSetAcl( inode, default_acl, ACL_TYPE_DEFAULT );
		posix_acl_release( default_acl );
	}

	if( acl )
	{
		if( !error )
		{
			error = me2fsSetAcl( inode, acl, ACL_TYPE_ACCESS );
		}

		posix_acl_release(acl);
	}

	return( error );
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
	Function	:getAclSize
	Input		:int count
				 < number of acls >
	Output		:void
	Return		:size_t
				 < size of acls >

	Description	:get size of acls
==================================================================================
*/
static inline size_t getAclSize( int count )
{
	if( count <= 4 )
	{
		return( sizeof( struct ext2_acl_header ) +
				count * sizeof( struct ext2_acl_entry_short ) );
	}
	else
	{
		return( sizeof( struct ext2_acl_header ) +
				4 * sizeof( struct ext2_acl_entry_short ) +
				( count - 4 ) * sizeof( struct ext2_acl_entry ) );
	}
}
/*
==================================================================================
	Function	:getAclCount
	Input		:size_t size
				 < size of acls >
	Output		:void
	Return		:int
				 < number of acls >

	Description	:get number of acls
==================================================================================
*/
static inline int getAclCount( size_t size )
{
	ssize_t		ent_size;

	size -= sizeof( struct ext2_acl_header );

	ent_size = size - 4 * sizeof( struct ext2_acl_entry_short );

	if( ent_size < 0 )
	{
		if( size % sizeof( struct ext2_acl_entry_short ) )
		{
			return( -1 );
		}

		return( size / sizeof( struct ext2_acl_entry_short ) );
	}
	else
	{
		if( ent_size % sizeof( struct ext2_acl_entry ) )
		{
			return( -1 );
		}

		return( ent_size / sizeof( struct ext2_acl_entry ) + 4 );
	}
}
/*
==================================================================================
	Function	:aclFromDisk
	Input		:const void *value
				 < value of acl >
				 size_t size
				 < size of value >
	Output		:void
	Return		:void

	Description	:convert from filesystem to in-memory representation
==================================================================================
*/
static struct posix_acl* aclFromDisk( const void* value, size_t size )
{
	char				*end;
	int					index;
	int					count;
	struct posix_acl	*acl;

	if( !value )
	{
		DBGPRINT( "<ME2FS>%s:value is null\n", __func__ );
		return( NULL );
	}

	if( size < sizeof( struct ext2_acl_header ) )
	{
		return( ERR_PTR( -EINVAL ) );
	}

	if( ( ( struct ext2_acl_header* )value )->a_version !=
		( cpu_to_le32( EXT2_ACL_VERSION ) ) )
	{
		return( ERR_PTR( -EINVAL ) );
	}

	count = getAclCount( size );

	if( count < 0 )
	{
		return( ERR_PTR( -EINVAL ) );
	}

	if( count == 0 );
	{
		DBGPRINT( "<ME2FS>%s:value is null2\n", __func__ );
		return( NULL );
	}

	if( !( acl = posix_acl_alloc( count, GFP_KERNEL ) ) )
	{
		return( ERR_PTR( -ENOMEM ) );
	}

	end		= ( char* )value + size;
	value	= ( char* )value + sizeof( struct ext2_acl_header );

	for( index = 0 ; index < count ; index++ )
	{
		struct ext2_acl_entry	*entry;

		entry = ( struct ext2_acl_entry* )value;

		if( end < ( ( char* )value + sizeof( struct ext2_acl_entry_short ) ) )
		{
			goto fail;
		}

		acl->a_entries[ index ].e_tag	= le16_to_cpu( entry->e_tag );
		acl->a_entries[ index ].e_perm	= le16_to_cpu( entry->e_perm );

		switch( acl->a_entries[ index ].e_tag )
		{
		case	ACL_USER_OBJ:
		case	ACL_GROUP_OBJ:
		case	ACL_MASK:
		case	ACL_OTHER:
			value = ( char* )value + sizeof( struct ext2_acl_entry_short );
			break;
		case	ACL_USER:
			value = ( char* )value + sizeof( struct ext2_acl_entry );
			if( end < ( char* )value )
			{
				goto fail;
			}
			acl->a_entries[ index ].e_uid
						= make_kuid( &init_user_ns, le32_to_cpu( entry->e_id ) );
			break;
		case	ACL_GROUP:
			value = ( char* )value + sizeof( struct ext2_acl_entry );
			if( end < ( char* )value )
			{
				goto fail;
			}
			acl->a_entries[ index ].e_gid
						= make_kgid( &init_user_ns, le32_to_cpu( entry->e_id ) );
			break;
		default:
			goto fail;
		}
	}

	if( value != end )
	{
		goto fail;
	}

	return( acl );

fail:
	posix_acl_release( acl );
	return( ERR_PTR( -EINVAL ) );
}
/*
==================================================================================
	Function	:aclToDisk
	Input		:const struct posix_acl *acl
				 < posix acls >
				 sizet_t *size
				 < size of acls >
	Output		:void
	Return		:void*
				 < addoress of ext2 acls >

	Description	:convert from in-memory to filesystem representaion
==================================================================================
*/
static void* aclToDisk( const struct posix_acl *acl, size_t *size )
{
	struct ext2_acl_header	*ext_acl;
	char					*next_entry;
	unsigned int			index;

	*size	= getAclSize( acl->a_count );

	ext_acl	= kmalloc( sizeof( struct ext2_acl_header ) +
					   ( acl->a_count * sizeof( struct ext2_acl_entry ) ),
					   GFP_KERNEL );
	
	if( !ext_acl )
	{
		return( ERR_PTR( -ENOMEM ) );
	}

	ext_acl->a_version	= cpu_to_le32( EXT2_ACL_VERSION );

	next_entry			= ( char* )ext_acl + sizeof( struct ext2_acl_header );

	for( index = 0 ; index < acl->a_count ; index++ )
	{
		const struct posix_acl_entry	*acl_e = &acl->a_entries[ index ];
		struct ext2_acl_entry			*entry;

		entry = ( struct ext2_acl_entry* )next_entry;

		entry->e_tag	= cpu_to_le16( acl_e->e_tag );
		entry->e_perm	= cpu_to_le16( acl_e->e_perm );

		switch( acl_e->e_tag )
		{
		case	ACL_USER:
			entry->e_id	= cpu_to_le32( from_kuid( &init_user_ns, acl_e->e_uid ) );
			next_entry	+= sizeof( struct ext2_acl_entry );
			break;
		case	ACL_GROUP:
			entry->e_id	= cpu_to_le32( from_kgid( &init_user_ns, acl_e->e_gid ) );
			next_entry	+= sizeof( struct ext2_acl_entry );
			break;
		case	ACL_USER_OBJ:
		case	ACL_GROUP_OBJ:
		case	ACL_MASK:
		case	ACL_OTHER:
			next_entry	+= sizeof( struct ext2_acl_entry_short );
			break;
		default:
			goto fail;
		}
	}

	return( ( void* )ext_acl );

fail:
	kfree( ext_acl );
	return( ERR_PTR( -EINVAL ) );
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
