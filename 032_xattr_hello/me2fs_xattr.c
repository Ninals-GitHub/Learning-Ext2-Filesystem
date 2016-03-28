/********************************************************************************
	File			: me2fs_xattr.c
	Description		: Operations for Extended Attribute

*********************************************************************************/
#include <linux/xattr.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_xattr.h"
#include "me2fs_xattr_user.h"
#include "me2fs_xattr_trusted.h"
#include "me2fs_xattr_security.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static inline struct xattr_handler* getXattrHandler( int name_index );

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
const struct xattr_handler *me2fs_xattr_handlers[ ] =
{
	&me2fs_xattr_user_handler,
	&me2fs_xattr_trusted_handler,
	&me2fs_xattr_security_handler,
	NULL,	// last of entry must be null
};

static const struct xattr_handler *me2fs_xattr_handler_map[ ] =
{
	[ EXT2_XATTR_INDEX_USER		]				= &me2fs_xattr_user_handler,
	[ EXT2_XATTR_INDEX_TRUSTED	]				= &me2fs_xattr_trusted_handler,
	[ EXT2_XATTR_INDEX_SECURITY	]				= &me2fs_xattr_security_handler,
};

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
				   size_t buffer_size )
{
	size_t	i;
	char	*buf;
	char	test_str[ ] = "my_xattr";

	DBGPRINT( "<ME2FS>:%s:get xattr (%d) [%s], buffer_size = %zu\n",
			  __func__, name_index, name, buffer_size );
	
	if( buffer_size < sizeof( test_str ) )
	{
		return( 0 );
	}

	buf = ( char* )buffer;

	for( i = 0 ; i < sizeof( test_str ) ; i++ )
	{
		buf[ i ] = test_str[ i ];
	}

	return( sizeof( test_str ) );
}

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
				   int flags )
{
	int i;

	DBGPRINT( "<ME2FS>:%s:set xattr(%d) [%s=",
			  __func__, name_index, name, ( char* )value );
	
	for( i = 0 ; i < value_len ; i++ )
	{
		DBGPRINT( "%c", *( ( char* )value + i ) );
	}

	DBGPRINT( "]\n" );

	return( 0 );
}
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
						size_t buffer_size )
{
	int						i;
	size_t					rest;
	struct xattr_handler	*handler;

	DBGPRINT( "<ME2FS>:%s:list xattr\n", __func__ );

	rest = buffer_size;

	for( i = 0 ; i < ARRAY_SIZE( me2fs_xattr_handler_map ) ; i++ )
	{
		handler = getXattrHandler( i );

		if( handler )
		{
			size_t	size;

			size = handler->list( dentry,
								  buffer,
								  rest,
								  "test",
								  sizeof( "test" ) - 1,
								  handler->flags );

			rest -= size;
		}
	}

	return( buffer_size - rest );
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
	Function	:getXattrHandler
	Input		:int name_index
				 < name index of xattr >
	Output		:void
	Return		:struct xattr_handler*
				 < xattr handler corresponding name index >

	Description	:
==================================================================================
*/
static inline struct xattr_handler* getXattrHandler( int name_index )
{
	if( ( 0 < name_index ) &&
		( name_index < ARRAY_SIZE( me2fs_xattr_handler_map ) ) )
	{
		return( ( struct xattr_handler* )me2fs_xattr_handler_map[ name_index ] );
	}

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
