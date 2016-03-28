/********************************************************************************
	File			: me2fs_xattr.c
	Description		: Operations for Extended Attribute

*********************************************************************************/
#include <linux/xattr.h>
#include <linux/buffer_head.h>
#include <linux/mbcache.h>
#include <linux/quotaops.h>
#include <linux/posix_acl_xattr.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_xattr.h"
#include "me2fs_xattr_user.h"
#include "me2fs_xattr_trusted.h"
#include "me2fs_xattr_security.h"
#include "me2fs_block.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static inline struct xattr_handler* getXattrHandler( int name_index );
static inline struct ext2_xattr_header*
getXattrHeader( struct buffer_head *bh );
static inline struct ext2_xattr_entry* getXattrEntry( char *ptr );
static inline struct ext2_xattr_entry*
getXattrNext( struct ext2_xattr_entry *entry );
static inline struct ext2_xattr_entry*
getXattrFirstEntry( struct buffer_head *bh );
static inline int isLastXattrEntry( struct ext2_xattr_entry *entry );
static inline int getXattrLen( int name_len );
static inline int getXattrSize( int size );
static int
xattrCacheInsert( struct buffer_head *bh );
static int
xattrCompare( struct ext2_xattr_header *head1, struct ext2_xattr_header *head2 );
static struct buffer_head*
xattrCacheFind( struct inode *inode, struct ext2_xattr_header *header );
static inline void
xattrHash( struct ext2_xattr_header *header, struct ext2_xattr_entry *entry );
static void xattrRehash( struct ext2_xattr_header *header,
						 struct ext2_xattr_entry *entry );
static int xattrSet2( struct inode *inode,
					  struct buffer_head *old_bh,
					  struct ext2_xattr_header *header );
static void xattrUpdateSuperBlock( struct super_block *sb );

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
	&posix_acl_access_xattr_handler,
	&posix_acl_default_xattr_handler,
	&me2fs_xattr_security_handler,
	NULL,	// last of entry must be null
};

static const struct xattr_handler *me2fs_xattr_handler_map[ ] =
{
	[ EXT2_XATTR_INDEX_USER				]	= &me2fs_xattr_user_handler,
	[ EXT2_XATTR_INDEX_POSIX_ACL_ACCESS	]	= &posix_acl_access_xattr_handler,
	[ EXT2_XATTR_INDEX_POSIX_ACL_DEFAULT]	= &posix_acl_default_xattr_handler,
	[ EXT2_XATTR_INDEX_TRUSTED			]	= &me2fs_xattr_trusted_handler,
	[ EXT2_XATTR_INDEX_SECURITY			]	= &me2fs_xattr_security_handler,
};

static struct mb_cache *me2fs_xattr_mbcache;
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
	struct buffer_head			*bh;
	struct ext2_xattr_header	*header;
	struct ext2_xattr_entry		*entry;
	size_t						name_len;
	size_t						size;
	char						*end;
	int							error;

	DBGPRINT( "<ME2FS>:%s:get xattr (%d) [%s], buffer_size = %zu\n",
			  __func__, name_index, name, buffer_size );

	if( !name )
	{
		return( -EINVAL );
	}

	name_len = strlen( name );

	if( 255 < name_len )
	{
		return( -ERANGE );
	}

	bh = NULL;

	down_read( &ME2FS_I( inode )->xattr_sem );

	error = -ENODATA;

	if( !ME2FS_I( inode )->i_file_acl )
	{
		goto cleanup;
	}

	DBGPRINT( "<ME2FS>%s: read xattr block %u\n",
			  __func__, ME2FS_I( inode )->i_file_acl );

	/* ------------------------------------------------------------------------ */
	/* read a block of xattr													*/
	/* ------------------------------------------------------------------------ */
	bh = sb_bread( inode->i_sb, ME2FS_I( inode )->i_file_acl );

	if( !bh )
	{
		error = -EIO;
		goto cleanup;
	}

	header = getXattrHeader( bh );

	DBGPRINT( "<ME2FS>%s:xattr b_count = %d, refcount = %d\n",
			  __func__,
			  atomic_read( &( bh->b_count ) ),
			  le32_to_cpu( header->h_refcount ) );
	
	end = bh->b_data + bh->b_size;

	if( ( header->h_magic != cpu_to_le32( EXT2_XATTR_MAGIC ) ) ||
		( header->h_blocks!= cpu_to_le32( 1 ) ) )
	{
		goto bad_block;
	}

	/* ------------------------------------------------------------------------ */
	/* find named attribute														*/
	/* ------------------------------------------------------------------------ */
	entry = getXattrFirstEntry( bh );

	while( !isLastXattrEntry ( entry ) )
	{
		struct ext2_xattr_entry *next;

		next = getXattrNext( entry );

		if( end <= ( char* )next )
		{
			error = -EIO;
			goto bad_block;
		}

		if( ( name_index	== entry->e_name_index	) &&
			( name_len		== entry->e_name_len	) &&
			( memcmp( name, entry->e_name, name_len ) == 0 ) )
		{
			goto found;
		}

		entry = next;
	}

	if( xattrCacheInsert( bh ) )
	{
		DBGPRINT( "<ME2FS>%s:cache insert failed\n", __func__ );
	}

	error = -ENODATA;

	DBGPRINT( "<ME2FS>%s:no attributes\n", __func__ );

	goto cleanup;

bad_block:
	ME2FS_ERROR( "<ME2FS>%s:error:ino = %ld, bad block %u\n",
				 __func__, inode->i_ino, ME2FS_I( inode )->i_file_acl );
	error = -EIO;
	goto cleanup;

found:
	/* ------------------------------------------------------------------------ */
	/* check the buffer size													*/
	/* ------------------------------------------------------------------------ */
	if( entry->e_value_block != 0 )
	{
		goto bad_block;
	}

	size = le32_to_cpu( entry->e_value_size );

	if( ( inode->i_sb->s_blocksize < size ) ||
		( inode->i_sb->s_blocksize <
		  ( size + le16_to_cpu( entry->e_value_offs ) ) ) )
	{
		goto bad_block;
	}

	if( xattrCacheInsert( bh ) )
	{
		DBGPRINT( "<ME2FS>%s:cache insert failed\n", __func__ );
	}

	if( buffer )
	{
		if( buffer_size < size )
		{
			error = -ERANGE;
			goto cleanup;
		}
		/* -------------------------------------------------------------------- */
		/* return value of attribute											*/
		/* -------------------------------------------------------------------- */
		memcpy( buffer, bh->b_data + le16_to_cpu( entry->e_value_offs ), size );
		*( ( char* )buffer + size ) = '\0';
		DBGPRINT( "<ME2FS>%s:buffer has %s\n", __func__, ( char* )buffer );
	}

	error = size;

cleanup:
	brelse( bh );
	up_read( &ME2FS_I( inode )->xattr_sem );

	return( error );
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
	struct super_block			*sb;
	struct buffer_head			*bh;
	struct ext2_xattr_header	*header;
	struct ext2_xattr_entry		*here;
	struct ext2_xattr_entry		*last;
	size_t						name_len;
	size_t						free;
	size_t						min_offs;
	int							not_found;
	int							error;
	char						*end;

	sb			= inode->i_sb;
	bh			= NULL;
	header		= NULL;
	
	min_offs	= sb->s_blocksize;

	not_found	= 1;

	DBGPRINT( "<ME2FS>:%s:set xattr(%d) [%s=%s]\n",
			  __func__, name_index, name, ( char* )value );

	/* ------------------------------------------------------------------------ */
	/* haeder   : points either into bh, or to a temporarily allocated header	*/
	/* here     : the named entry found, or the place for inserting, within		*/
	/*            the block pointed to by the header							*/
	/* last     : points right after the last named entry within the block		*/
	/*            pointed to by the header										*/
	/* min_offs : the offset of the first value (values are aligned toward		*/
	/*            the end of the block)											*/
	/* end      : points right after the block pointed to by the header			*/
	/* ------------------------------------------------------------------------ */
	if( !name )
	{
		return( -EINVAL );
	}

	if( !value )
	{
		value_len = 0;
	}

	name_len = strlen( name );

	if( ( 255 < name_len ) ||
		( sb->s_blocksize < value_len ) )
	{
		return( -ERANGE );
	}

	down_write( &ME2FS_I( inode )->xattr_sem );

	DBGPRINT( "<ME2FS>%s:down_write\n", __func__ );

	if( ME2FS_I( inode )->i_file_acl )
	{
		DBGPRINT( "<ME2FS>%s:already has xattr block\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* the inode already has an xattr block									*/
		/* -------------------------------------------------------------------- */
		bh		= sb_bread( sb, ME2FS_I( inode )->i_file_acl );
		error	= -EIO;

		if( !bh )
		{
			goto cleanup;
		}

		header	= getXattrHeader( bh );
		end		= bh->b_data + bh->b_size;

		if( ( header->h_magic != cpu_to_le32( EXT2_XATTR_MAGIC ) ) ||
			( header->h_blocks!= cpu_to_le32( 1 ) ) )
		{
			goto bad_block;
		}

		DBGPRINT( "<ME2FS>%s:find named attribute\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* find a named attribute												*/
		/* -------------------------------------------------------------------- */
		here = getXattrFirstEntry( bh );

		while( !isLastXattrEntry( here ) )
		{
			struct ext2_xattr_entry	*next;

			next = getXattrNext( here );

			if( end <= ( char* )next )
			{
				goto bad_block;
			}

			if( !here->e_value_block && here->e_value_size )
			{
				size_t	offs;

				offs = le16_to_cpu( here->e_value_offs );

				if( offs < min_offs )
				{
					min_offs = offs;
				}
			}

			not_found = name_index - here->e_name_index;

			if( !not_found )
			{
				not_found = name_len - here->e_name_len;
			}

			if( !not_found )
			{
				not_found = memcmp( name, here->e_name, name_len );
			}

			if( not_found <= 0 )
			{
				break;
			}

			here = next;
		}
		
		last = here;

		DBGPRINT( "<ME2FS>%s:compute min_offs and last\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* still need to compute min_offs and last								*/
		/* -------------------------------------------------------------------- */
		while( !isLastXattrEntry( last ) )
		{
			struct ext2_xattr_entry	*next;

			next = getXattrNext( last );

			if( end <= ( char* )next )
			{
				goto bad_block;
			}

			if( !last->e_value_block && last->e_value_size )
			{
				size_t	offs;

				offs = le16_to_cpu( last->e_value_offs );

				if( offs < min_offs )
				{
					min_offs = offs;
				}
			}

			last = next;
		}
		DBGPRINT( "<ME2FS>%s:check whether we have enough space left\n",
				  __func__ );
		/* -------------------------------------------------------------------- */
		/* check whether we have enough space left								*/
		/* -------------------------------------------------------------------- */
		free = min_offs - ( ( char* )last - ( char* )header ) - sizeof( __u32 );
	}
	else
	{
		DBGPRINT( "<ME2FS>%s:new block is needed\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* a new block is needed												*/
		/* -------------------------------------------------------------------- */
		free = sb->s_blocksize -
			   sizeof( struct ext2_xattr_header ) -
			   sizeof( __u32 );

		/* avoid gcc warning													*/
		here = last = NULL;
	}

	if( not_found )
	{
		DBGPRINT( "<ME2FS>%s:request to remove nonexistent\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* request to remove a nonexistent attribute?							*/
		/* -------------------------------------------------------------------- */
		if( flags & XATTR_REPLACE )
		{
			error = -ENODATA;
			goto cleanup;
		}

		error = 0;
		if( !value )
		{
			goto cleanup;
		}
	}
	else
	{
		DBGPRINT( "<ME2FS>%s:request to create nexistng\n", __func__ );
		/* -------------------------------------------------------------------- */
		/* request to create a existing attribute?								*/
		/* -------------------------------------------------------------------- */
		error = -EEXIST;
		if( flags & XATTR_CREATE )
		{
			goto cleanup;
		}

		if( !here->e_value_block && here->e_value_size )
		{
			size_t	size;

			size = le32_to_cpu( here->e_value_size );

			if( ( sb->s_blocksize < size ) ||
				( sb->s_blocksize <
				  ( le16_to_cpu( here->e_value_offs ) + size ) ) )
			{
				goto bad_block;
			}

			free += getXattrSize( size );
		}

		free += getXattrLen( name_len );
	}

	error = -ENOSPC;
	if( free < ( getXattrLen( name_len ) + getXattrSize( value_len ) ) )
	{
		goto cleanup;
	}

	DBGPRINT( "<ME2FS>%s:set the new attribute\n", __func__ );

	/* ------------------------------------------------------------------------ */
	/* here we know that we can set the new attribute							*/
	/* ------------------------------------------------------------------------ */
	if( header )
	{
		struct mb_cache_entry	*ce;

		/* -------------------------------------------------------------------- */
		/* assert(header == getXattrHeader( bh ) );								*/
		/* -------------------------------------------------------------------- */
		ce = mb_cache_entry_get( me2fs_xattr_mbcache,
								 bh->b_bdev,
								 bh->b_blocknr );

		lock_buffer( bh );

		if( header->h_refcount == cpu_to_le32( 1 ) )
		{
			DBGPRINT( "<ME2FS>%s:modifying in-place\n", __func__ );
			if( ce )
			{
				mb_cache_entry_free( ce );
			}
			/* keep the buffer locked while modifying it						*/
		}
		else
		{
			int	offset;

			if( ce )
			{
				mb_cache_entry_release( ce );
			}

			unlock_buffer( bh );

			error = -ENOMEM;
			if( !( header = kmalloc( bh->b_size, GFP_KERNEL ) ) )
			{
				goto cleanup;
			}

			memcpy( header, getXattrHeader( bh ), bh->b_size );

			header->h_refcount = cpu_to_le32( 1 );

			offset	= ( char* )here - bh->b_data;
			here	= getXattrEntry( ( char* )header + offset );
			offset	= ( char* )last - bh->b_data;
			last	= getXattrEntry( ( char* )header + offset );
		}
	}
	else
	{
		/* -------------------------------------------------------------------- */
		/* allocate a buffer where we construct the new block					*/
		/* -------------------------------------------------------------------- */
		error = -ENOMEM;
		if( !( header = kzalloc( sb->s_blocksize, GFP_KERNEL ) ) )
		{
			goto cleanup;
		}
		end					= ( char* )header + sb->s_blocksize;
		header->h_magic		= cpu_to_le32( EXT2_XATTR_MAGIC );
		header->h_blocks	= cpu_to_le32( 1 );
		header->h_refcount	= cpu_to_le32( 1 );
		last				= getXattrEntry( ( char* )( header + 1 ) );
		here				= last;
	}

	DBGPRINT( "<ME2FS>%s:modifying block\n", __func__ );
	/* ------------------------------------------------------------------------ */
	/* if we are modifying the block in-place, bh is locked here				*/
	/* ------------------------------------------------------------------------ */
	if( not_found )
	{
		/* -------------------------------------------------------------------- */
		/* insert the new name													*/
		/* -------------------------------------------------------------------- */
		size_t	size;
		size_t	rest;

		size = getXattrLen( name_len );
		rest = ( char* )last - ( char* )here;

		memmove( ( char* )here + size, here, rest );
		memset( here, 0, size );

		here->e_name_index	= name_index;
		here->e_name_len	= name_len;

		memcpy( here->e_name, name, name_len );
	}
	else
	{
		if( !here->e_value_block && here->e_value_size )
		{
			char	*first_val;
			size_t	offs;
			char	*val;
			size_t	size;

			first_val	= ( char* )header + min_offs;
			offs		= le16_to_cpu( here->e_value_offs );
			val			= ( char* )header + offs;
			size		= getXattrSize( le32_to_cpu( here->e_value_size ) );

			if( size == getXattrSize( value_len ) )
			{
				/* ------------------------------------------------------------ */
				/* the old and the new value have the same size. just replace	*/
				/* ------------------------------------------------------------ */
				here->e_value_size = cpu_to_le32( value_len );
				/* clear pad bytes												*/
				memset( val + size - EXT2_XATTR_PAD, 0, EXT2_XATTR_PAD );

				memcpy( val, value, value_len );
				goto skip_replace;
			}

			/* ---------------------------------------------------------------- */
			/* remove the old value												*/
			/* ---------------------------------------------------------------- */
			memmove( first_val + size, first_val, val - first_val );
			memset( first_val, 0, size );
			here->e_value_offs = 0;
			min_offs += size;
			/* ---------------------------------------------------------------- */
			/* adjust all value offsets											*/
			/* ---------------------------------------------------------------- */
			last = getXattrEntry( ( char* )( header + 1 ) );
			while( !isLastXattrEntry( last ) )
			{
				size_t	adj_off;

				adj_off = le16_to_cpu( last->e_value_offs );
				if( !last->e_value_block && ( adj_off < offs ) )
				{
					last->e_value_offs = cpu_to_le16( adj_off + size );
				}

				last = getXattrNext( last );
			}
		}

		if( !value )
		{
			/* ---------------------------------------------------------------- */
			/* remove the old name												*/
			/* ---------------------------------------------------------------- */
			size_t	size;

			size = getXattrLen( name_len );
			last = getXattrEntry( ( char* )last - size );

			memmove( here, ( char* )here + size, ( char* )last - ( char* )here );
			memset( last, 0, size );
		}
	}

	DBGPRINT( "<ME2FS>%s:insert the new value\n", __func__ );
	if( value )
	{
		/* -------------------------------------------------------------------- */
		/* insert the new value													*/
		/* -------------------------------------------------------------------- */
		here->e_value_size = cpu_to_le32( value_len );
		if( value_len )
		{
			size_t	size;
			char	*val;

			size	= getXattrSize( value_len );
			val		= ( char* )header + min_offs - size;
			
			here->e_value_offs = cpu_to_le16( ( char* )val - ( char* )header );

			memset( val + size - EXT2_XATTR_PAD, 0, EXT2_XATTR_PAD );
			memcpy( val, value, value_len );
		}
	}

skip_replace:
	DBGPRINT( "<ME2FS>%s:skip replace\n", __func__ );
	if( isLastXattrEntry( getXattrEntry( ( char* )( header + 1 ) ) ) )
	{
		DBGPRINT( "<ME2FS>%s:last entry\n", __func__ );

		/* -------------------------------------------------------------------- */
		/* this block is now empyt												*/
		/* -------------------------------------------------------------------- */
		if( bh && ( header == getXattrHeader( bh ) ) )
		{
			/* we were modifying in-place										*/
			unlock_buffer( bh );
		}

		error = xattrSet2( inode, bh, NULL );
	}
	else
	{
		DBGPRINT( "<ME2FS>%s:not last entry\n", __func__ );
		xattrRehash( header, here );
		if( bh && ( header == getXattrHeader( bh ) ) )
		{
			/* we were modifying in-place										*/
			unlock_buffer( bh );
		}

		error = xattrSet2( inode, bh, header );
	}

cleanup:
	brelse( bh );
	if( !( bh && ( header == getXattrHeader( bh ) ) ) )
	{
		kfree( header );
	}
	up_write( &ME2FS_I( inode )->xattr_sem );

	return( error );

bad_block:
	ME2FS_ERROR( "<ME2FS>%s:error: ino = %ld, bad block = %u\n",
				 __func__,
				 inode->i_ino,
				 ME2FS_I( inode )->i_file_acl );
	error = -EIO;
	goto cleanup;

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
	struct inode			*inode;
	struct buffer_head		*bh;
	struct ext2_xattr_entry	*entry;
	char					*end;
	size_t					rest;
	int						error;

	inode	= dentry->d_inode;
	bh		= NULL;

	rest	= buffer_size;

	DBGPRINT( "<ME2FS>:%s:list xattr\n", __func__ );

	down_read( &ME2FS_I( inode )->xattr_sem );

	error	= 0;
	if( !ME2FS_I( inode )->i_file_acl )
	{
		goto cleanup;
	}

	bh = sb_bread( inode->i_sb, ME2FS_I( inode )->i_file_acl );

	error = -EIO;

	if( !bh )
	{
		goto cleanup;
	}

	end = bh->b_data + bh->b_size;

	if( ( getXattrHeader( bh )->h_magic != cpu_to_le32( EXT2_XATTR_MAGIC ) ) ||
		( getXattrHeader( bh )->h_blocks!= cpu_to_le32( 1 ) ) )
	{
		goto bad_block;
	}

	/* ------------------------------------------------------------------------ */
	/* check the on-disk data structure											*/
	/* ------------------------------------------------------------------------ */
	entry = getXattrFirstEntry( bh );

	while( !isLastXattrEntry( entry ) )
	{
		struct ext2_xattr_entry	*next;

		next = getXattrNext( entry );

		if( end <= ( char* )next )
		{
			goto bad_block;
		}

		entry = next;
	}

	if( xattrCacheInsert( bh ) )
	{
		DBGPRINT( "<ME2Fs>%s:cache insert failed\n", __func__ );
	}

	/* ------------------------------------------------------------------------ */
	/* list the attribute names													*/
	/* ------------------------------------------------------------------------ */
	for( entry = getXattrFirstEntry( bh ) ;
		 !isLastXattrEntry( entry ) ;
		 entry = getXattrNext( entry ) )
	{
		struct xattr_handler *handler;
		
		handler = getXattrHandler( ( int )entry->e_name_index );

		if( handler )
		{
			size_t	size;

			size = handler->list( dentry,
								  buffer,
								  rest,
								  entry->e_name,
								  entry->e_name_len,
								  handler->flags );

			if( buffer )
			{
				if( rest < size )
				{
					error = -ERANGE;
					goto cleanup;
				}

				buffer += size;
			}

			rest -= size;
		}
	}

	/* total size																*/
	error = buffer_size - rest;

cleanup:
	brelse( bh );
	up_read( &ME2FS_I( inode )->xattr_sem );

	return( error );

bad_block:
	ME2FS_ERROR( "<ME2FS>%s:error: ino = %ld, bad block = %u\n",
				 __func__,
				 inode->i_ino,
				 ME2FS_I( inode )->i_file_acl );
	error = -EIO;
	goto cleanup;
	
}
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
void me2fsDeleteXattr( struct inode *inode )
{
	struct buffer_head		*bh;
	struct mb_cache_entry	*ce;

	DBGPRINT( "<ME2FS>%s:delete extended attribute(%ld)\n",
			  __func__, inode->i_ino );

	down_write( &ME2FS_I( inode )->xattr_sem );

	if( !ME2FS_I( inode )->i_file_acl )
	{
		bh = NULL;
		goto cleanup;
	}

	bh = sb_bread( inode->i_sb, ME2FS_I( inode )->i_file_acl );

	if( !bh )
	{
		ME2FS_ERROR( "<ME2FS>%s:error: inode = %ld, block %d read error\n",
					 __func__, inode->i_ino, ME2FS_I( inode )->i_file_acl );
		goto cleanup;
	}

	if( ( getXattrHeader( bh )->h_magic  != cpu_to_le32( EXT2_XATTR_MAGIC ) ) ||
		( getXattrHeader( bh )->h_blocks != cpu_to_le32( 1 ) ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:error: inode = %ld, block %d bad block\n",
					 __func__, inode->i_ino, ME2FS_I( inode )->i_file_acl );
		goto cleanup;
	}

	ce = mb_cache_entry_get( me2fs_xattr_mbcache, bh->b_bdev, bh->b_blocknr );

	lock_buffer( bh );

	if( getXattrHeader( bh )->h_refcount == cpu_to_le32( 1 ) )
	{
		if( ce )
		{
			mb_cache_entry_free( ce );
		}

		me2fsFreeBlocks( inode, ME2FS_I( inode )->i_file_acl, 1 );
		get_bh( bh );
		bforget( bh );
		unlock_buffer( bh );
	}
	else
	{
		le32_add_cpu( &getXattrHeader( bh )->h_refcount, -1 );
		if( ce )
		{
			mb_cache_entry_release( ce );
		}
		unlock_buffer( bh );
		mark_buffer_dirty( bh );
		if( IS_SYNC( inode ) )
		{
			sync_dirty_buffer( bh );
		}
		dquot_free_block_nodirty( inode, 1 );
	}

	ME2FS_I( inode )->i_file_acl = 0;

cleanup:
	brelse( bh );
	up_write( &ME2FS_I( inode )->xattr_sem );
}
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
void me2fsXattrPutSuper( struct super_block *sb )
{
	mb_cache_shrink( sb->s_bdev );
}
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
int me2fsInitXattr( void )
{
	me2fs_xattr_mbcache = mb_cache_create( "me2fs_xattr", 6 );

	if( !me2fs_xattr_mbcache )
	{
		return( -ENOMEM );
	}

	return( 0 );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsExitXattr
	Input		:void
	Output		:void
	Return		:void

	Description	:destry xattr mbcache
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsExitXattr( void )
{
	mb_cache_destroy( me2fs_xattr_mbcache );
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
	Function	:getXattrHeader
	Input		:struct buffer_head *bh
				 < buffer cache contains header of xattr >
	Output		:void
	Return		:struct ext2_xattr_header*
				 < address of xattr header >

	Description	:get address of xattr header
==================================================================================
*/
static inline struct ext2_xattr_header*
getXattrHeader( struct buffer_head *bh )
{
	return( ( struct ext2_xattr_header* )bh->b_data );
}
/*
==================================================================================
	Function	:getXattrEntry
	Input		:char *ptr
	Output		:void
	Return		:struct ext2_xattr_entry*
				 < address of xattr entry >

	Description	:get address of xattr entry
==================================================================================
*/
static inline struct ext2_xattr_entry* getXattrEntry( char *ptr )
{
	return( ( struct ext2_xattr_entry* )ptr );
}
/*
==================================================================================
	Function	:getXattrNext
	Input		:struct ext2_xattr_entry *entry
				 < current entry >
	Output		:void
	Return		:struct ext2_xattr_entry*
				 < next entry >

	Description	:get next entry
==================================================================================
*/
static inline struct ext2_xattr_entry*
getXattrNext( struct ext2_xattr_entry *entry )
{
	return( ( struct ext2_xattr_entry* )
			( ( char* )entry + getXattrLen( entry->e_name_len ) ) );
}

/*
==================================================================================
	Function	:getXattrFirstEntry
	Input		:struct buffer_head *bh
				 < buffer cache contains header of xattr >
	Output		:void
	Return		:struct ext2_xattr_entry*
				 < first xattr entry >

	Description	:get first xattr entry
==================================================================================
*/
static inline struct ext2_xattr_entry*
getXattrFirstEntry( struct buffer_head *bh )
{
	return( ( struct ext2_xattr_entry* )( getXattrHeader( bh ) + 1 ) );
}

/*
==================================================================================
	Function	:isLastXattrEntry
	Input		:struct ext2_xattr_entry *entry
				 < xattr entry to test >
	Output		:void
	Return		:int
				 < result >

	Description	:test if the given entry is last
==================================================================================
*/
static inline int isLastXattrEntry( struct ext2_xattr_entry *entry )
{
	return( *( __u32* )( entry ) == 0 );
}
/*
==================================================================================
	Function	:getXattrLen
	Input		:int name_len
	Output		:void
	Return		:int
				 < rounded length of name >

	Description	:calculate rounded lenght of name
==================================================================================
*/
static inline int getXattrLen( int name_len )
{
	int	xttr_len;

	xttr_len = name_len + EXT2_XATTR_ROUND + sizeof( struct ext2_xattr_entry );
	xttr_len &= ~EXT2_XATTR_ROUND;

	return( xttr_len );
}

/*
==================================================================================
	Function	:getXattrSize
	Input		:int size
	Output		:void
	Return		:int
				 < rounded size >

	Description	:round size of xattr and get it
==================================================================================
*/
static inline int getXattrSize( int size )
{
	return( ( size + EXT2_XATTR_ROUND ) & ~EXT2_XATTR_ROUND );
}
/*
==================================================================================
	Function	:xattrCacheInsert
	Input		:struct buffer_head *bh
	Output		:void
	Return		:void

	Description	:create a new entry in the extended attribute cache, and
				 insert it unless such an entry is already in the cache.
==================================================================================
*/
static int
xattrCacheInsert( struct buffer_head *bh )
{
	__u32					hash;
	struct mb_cache_entry	*ce;
	int						error;

	if( !( ce = mb_cache_entry_alloc( me2fs_xattr_mbcache, GFP_NOFS ) ) )
	{
		return( -ENOMEM );
	}

	hash = le32_to_cpu( getXattrHeader( bh )->h_hash );

	if( ( error = mb_cache_entry_insert( ce, bh->b_bdev, bh->b_blocknr, hash ) ) )
	{
		mb_cache_entry_free( ce );
		if( error == -EBUSY )
		{
			DBGPRINT( "<ME2FS>%s:already in cache(%d cache entries)\n",
					  __func__,
					  atomic_read( &me2fs_xattr_mbcache->c_entry_count ) );
			error = 0;
		}
	}
	else
	{
		DBGPRINT( "<ME2FS>%s:inserting [%x] (%d cache entries)\n",
				  __func__, ( int )hash,
				  atomic_read( &me2fs_xattr_mbcache->c_entry_count ) );
		mb_cache_entry_release( ce );
	}
	
	return( error );
}
/*
==================================================================================
	Function	:xattrCompare
	Input		:struct ext2_xattr_header *head1
				 < target for comparison >
				 struct ext2_xattr_header *head2
				 < target for comparison >
	Output		:void
	Return		:int
				 < result 0:blocks are equal, 1:blocks differ, negative:error >

	Description	:compare two extended attribute blocks for equality.
==================================================================================
*/
static int
xattrCompare( struct ext2_xattr_header *head1, struct ext2_xattr_header *head2 )
{
	struct ext2_xattr_entry	*entry1;
	struct ext2_xattr_entry	*entry2;

	entry1 = getXattrEntry( ( char* )( head1 + 1 ) );
	entry2 = getXattrEntry( ( char* )( head2 + 1 ) );

	while( !isLastXattrEntry( entry1 ) )
	{
		if( isLastXattrEntry( entry2 ) )
		{
			return( 1 );
		}

		if( ( entry1->e_hash		!= entry2->e_hash		)	||
			( entry1->e_name_index	!= entry2->e_name_index	)	||
			( entry1->e_name_len	!= entry2->e_name_len	)	||
			( entry1->e_value_size	!= entry2->e_value_size	) )
		{
			if( memcmp( entry1->e_name, entry2->e_name, entry1->e_name_len ) )
			{
				return( 1 );
			}
		}

		if( ( entry1->e_value_block	!= 0 ) ||
			( entry2->e_value_block	!= 0 ) )
		{
			return( -EIO );
		}

		if( memcmp( ( char* )head1 + le16_to_cpu( entry1->e_value_offs ),
					( char* )head2 + le16_to_cpu( entry2->e_value_offs ),
					le16_to_cpu( entry1->e_value_size ) ) )
		{
			return( 1 );
		}

		entry1 = getXattrNext( entry1 );
		entry2 = getXattrNext( entry2 );
	}

	if( !isLastXattrEntry( entry2 ) )
	{
		return( 1 );
	}

	return( 0 );
}
/*
==================================================================================
	Function	:xattrCacheFind
	Input		:struct inode *inode
				 < vfs inode >
				 struct ext2_xattr_header *header
				 < header of xattr >
	Output		:void
	Return		:struct buffer_head*
				 < buffer head contains the given header >

	Description	:find an identical xattr block
==================================================================================
*/
static struct buffer_head*
xattrCacheFind( struct inode *inode, struct ext2_xattr_header *header )
{
	__u32					hash;
	struct mb_cache_entry	*ce;
	struct block_device		*bdev;

	if( !header->h_hash )
	{
		return( NULL );
	}

	hash = le32_to_cpu( header->h_hash );
	bdev = inode->i_sb->s_bdev;

again:
	ce = mb_cache_entry_find_first( me2fs_xattr_mbcache, bdev, hash );

	while( ce )
	{
		struct buffer_head	*bh;

		if( IS_ERR( ce ) )
		{
			if( PTR_ERR( ce ) == -EAGAIN )
			{
				goto again;
			}

			break;
		}

		if( !( bh = sb_bread( inode->i_sb, ce->e_block ) ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:read error:inode %ld:block %ld\n",
						 __func__, inode->i_ino, ( unsigned long )ce->e_block );
		}
		else
		{
			lock_buffer( bh );
			{
				if( EXT2_XATTR_REFCOUNT_MAX <
					le32_to_cpu( getXattrHeader( bh )->h_refcount ) )
				{
					DBGPRINT( "<ME2FS>%s:block %ld refcount %d<%d\n",
							  __func__,
							  ( unsigned long )ce->e_block,
							  le32_to_cpu( getXattrHeader( bh )->h_refcount ),
							  EXT2_XATTR_REFCOUNT_MAX );
				}
				else if( !xattrCompare( header, getXattrHeader( bh ) ) )
				{
					DBGPRINT( "<ME2FS>%s: b_count = %d\n",
							  __func__, atomic_read( &( bh->b_count ) ) );
					mb_cache_entry_release( ce );
					return( bh );
				}
			}
			unlock_buffer( bh );
			brelse( bh );
		}
		ce = mb_cache_entry_find_next( ce, bdev, hash );
	}

	return( NULL );
}
/*
==================================================================================
	Function	:xattrHash
	Input		:struct ext2_xattr_header *header
				 < header of xattr >
				 struct ext2_xattr_entry *entry
				 < xattr entry >
	Output		:void
	Return		:void

	Description	:calculate hash of xattr
==================================================================================
*/
static inline void
xattrHash( struct ext2_xattr_header *header, struct ext2_xattr_entry *entry )
{
	#define	NAME_HASH_SHIFT		5
	#define	VALUE_HASH_SHIFT	16

	__u32	hash;
	char	*name;
	int		n;

	hash = 0;
	name = entry->e_name;

	for( n = 0 ; n < entry->e_name_len ; n++ )
	{
		hash = ( hash << NAME_HASH_SHIFT ) ^
			   ( hash >> ( 8 * sizeof( hash ) - NAME_HASH_SHIFT ) ) ^
			   *name++;
	}

	if( ( entry->e_value_block	== 0 ) &&
		( entry->e_value_size	!= 0 ) )
	{
		__le32	*value;

		value =
			( __le32* )( ( char* )header + le16_to_cpu( entry->e_value_offs ) );

		for( n = ( le32_to_cpu( entry->e_value_size ) + EXT2_XATTR_ROUND ) >>
				 EXT2_XATTR_PAD_BITS ;
			 n ;
			 n-- )
		{
			hash = ( hash << NAME_HASH_SHIFT ) ^
				   ( hash >> ( 8 * sizeof( hash ) - NAME_HASH_SHIFT ) ) ^
				   le32_to_cpu( *value++ );
		}
	}

	entry->e_hash = cpu_to_le32( hash );

	#undef	NAME_HASH_SHIFT
	#undef	VALUE_HASH_SHIFT
}
/*
==================================================================================
	Function	:xattrRehash
	Input		:struct ext2_xattr_header *header
				 < header of xattr >
				 struct ext2_xattr_entry *entry
				 < xattr entry >
	Output		:void
	Return		:void

	Description	:re-compute xattr hash value after an entry has changed
==================================================================================
*/
static void xattrRehash( struct ext2_xattr_header *header,
						 struct ext2_xattr_entry *entry )
{
	#define	BLOCK_HASH_SHIFT	16
	struct ext2_xattr_entry	*here;
	__u32					hash;

	hash = 0;

	xattrHash( header, entry );
	here = getXattrEntry( ( char* )( header + 1 ) );
	while( !isLastXattrEntry( here ) )
	{
		if( !here->e_hash )
		{
			/* block is not shared if an entry's hash value = 0					*/
			hash = 0;
			break;
		}

		hash = ( hash << BLOCK_HASH_SHIFT ) ^
			   ( hash >> ( 8 * sizeof( hash ) - BLOCK_HASH_SHIFT ) ) ^
			   le32_to_cpu( here->e_hash );
		
		here = getXattrNext( here );
	}
	header->h_hash = cpu_to_le32( hash );
	#undef	BLOCK_HASH_SHIFT
}
/*
==================================================================================
	Function	:xattrSet2
	Input		:struct inode *inode
				 < vfs inode >
				 struct buffer_head *old_bh
				 < buffer cache >
				 struct ext2_xattr_header *header
				 < xattr header >
	Output		:void
	Return		:int
				 < result >

	Description	:second half of me2fsSetXattr() : update the file system
==================================================================================
*/
static int xattrSet2( struct inode *inode,
					  struct buffer_head *old_bh,
					  struct ext2_xattr_header *header )
{
	struct super_block	*sb;
	struct buffer_head	*new_bh;
	int					error;

	sb		= inode->i_sb;
	new_bh	= NULL;

	if( header )
	{
		new_bh = xattrCacheFind( inode, header );

		if( new_bh )
		{
			/* ---------------------------------------------------------------- */
			/* we found an identical block in the cache							*/
			/* ---------------------------------------------------------------- */
			if( new_bh == old_bh )
			{
				DBGPRINT( "<ME2FS>%s:keeping this block\n", __func__ );
			}
			else
			{
				DBGPRINT( "<ME2FS>%s:reusing block\n", __func__ );
				/* ------------------------------------------------------------ */
				/* the old block is released after updating the inode			*/
				/* ------------------------------------------------------------ */
				error = dquot_alloc_block( inode, 1 );

				if( error )
				{
					unlock_buffer( new_bh );
					goto cleanup;
				}

				le32_add_cpu( &getXattrHeader( new_bh )->h_refcount, 1 );
			}
			unlock_buffer( new_bh );
		}
		else if( old_bh && ( header == getXattrHeader( old_bh ) ) )
		{
			/* ---------------------------------------------------------------- */
			/* keep this block. no need to lock the block as we don't need to	*/
			/* change the reference count										*/
			/* ---------------------------------------------------------------- */
			new_bh = old_bh;
			get_bh( new_bh );
			xattrCacheInsert( new_bh );
		}
		else
		{
			/* ---------------------------------------------------------------- */
			/* we need to allocate a new block									*/
			/* ---------------------------------------------------------------- */
			unsigned long	goal;
			int				block;
			unsigned long	count;

			goal	= ext2GetFirstBlockNum( sb, ME2FS_I( inode )->i_block_group );

			count	= 1;
			block	= me2fsNewBlocks( inode, goal, &count, &error );

			if( error )
			{
				goto cleanup;
			}

			new_bh = sb_getblk( sb, block );

			if( unlikely( !new_bh ) )
			{
				me2fsFreeBlocks( inode, block, 1 );
				mark_inode_dirty( inode );
				error = -ENOMEM;
				goto cleanup;
			}

			lock_buffer( new_bh );
			{
				memcpy( new_bh->b_data, header, new_bh->b_size );
				set_buffer_uptodate( new_bh );
			}
			unlock_buffer( new_bh );

			xattrCacheInsert( new_bh );
			xattrUpdateSuperBlock( sb );
		}
		mark_buffer_dirty( new_bh );

		if( IS_SYNC( inode ) )
		{
			sync_dirty_buffer( new_bh );
			error = -EIO;
			if( buffer_req( new_bh ) && !buffer_uptodate( new_bh ) )
			{
				goto cleanup;
			}
		}
	}

	/* ------------------------------------------------------------------------ */
	/* update the inode															*/
	/* ------------------------------------------------------------------------ */
	if( new_bh )
	{
		ME2FS_I( inode )->i_file_acl = new_bh->b_blocknr;
	}
	else
	{
		ME2FS_I( inode )->i_file_acl = 0;
	}

	inode->i_ctime = CURRENT_TIME_SEC;

	if( IS_SYNC( inode ) )
	{
		error = sync_inode_metadata( inode, 1 );
		/* -------------------------------------------------------------------- */
		/* in case sync failed due to ENOSPC the inode was actually written		*/
		/* (only some dirty data were not) so we just proceed as if nothing		*/
		/* happened and cleanup the unused block								*/
		/* -------------------------------------------------------------------- */
		if( error && ( error != -ENOSPC ) )
		{
			if( new_bh && ( new_bh != old_bh ) )
			{
				dquot_free_block_nodirty( inode, 1 );
				mark_inode_dirty( inode );
			}
			goto cleanup;
		}
	}
	else
	{
		mark_inode_dirty( inode );
	}

	error = 0;

	if( old_bh && ( old_bh != new_bh ) )
	{
		struct mb_cache_entry	*ce;
		/* -------------------------------------------------------------------- */
		/* if there was an old block and we are no longer using it, release		*/
		/* the old block														*/
		/* -------------------------------------------------------------------- */
		ce = mb_cache_entry_get( me2fs_xattr_mbcache,
								 old_bh->b_bdev,
								 old_bh->b_blocknr );

		lock_buffer( old_bh );
		{
			if( getXattrHeader( old_bh )->h_refcount == cpu_to_le32( 1 ) )
			{
				/* ------------------------------------------------------------ */
				/* free the old block											*/
				/* ------------------------------------------------------------ */
				if( ce )
				{
					mb_cache_entry_free( ce );
				}

				me2fsFreeBlocks( inode, old_bh->b_blocknr, 1 );
				mark_inode_dirty( inode );
				/* ------------------------------------------------------------ */
				/* we let our caller release old_bh, so we need to duplicate	*/
				/* the buffer before											*/
				/* ------------------------------------------------------------ */
				get_bh( old_bh );
				bforget( old_bh );
			}
			else
			{
				/* ------------------------------------------------------------ */
				/* decrement the refcount only									*/
				/* ------------------------------------------------------------ */
				le32_add_cpu( &getXattrHeader( old_bh )->h_refcount, -1 );

				if( ce )
				{
					mb_cache_entry_release( ce );
				}
				dquot_free_block_nodirty( inode, 1 );
				mark_inode_dirty( inode );
				mark_buffer_dirty( old_bh );
			}
		}
		unlock_buffer( old_bh );
	}

cleanup:
	brelse( new_bh );
	
	return( error );
}
/*
==================================================================================
	Function	:xattrUpdateSuperBlock
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:update super block
==================================================================================
*/
static void xattrUpdateSuperBlock( struct super_block *sb )
{
	if( ME2FS_SB( sb )->s_esb->s_feature_compat &
		cpu_to_le32( EXT2_FEATURE_COMPAT_EXT_ATTR ) )
	{
		return;
	}

	spin_lock( &ME2FS_SB( sb )->s_lock );
	{
		ME2FS_SB( sb )->s_esb->s_feature_compat |=
				cpu_to_le32( EXT2_FEATURE_COMPAT_EXT_ATTR );
	}
	spin_unlock( &ME2FS_SB( sb )->s_lock );
	mark_buffer_dirty( ME2FS_SB( sb )->s_sbh );
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
