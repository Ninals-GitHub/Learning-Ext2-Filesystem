/********************************************************************************
	File			: me2fs_dir.c
	Description		: Directory Operations of my ext2 file system

*********************************************************************************/

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"
#include "me2fs_dir.h"



/*
==================================================================================

	Prototype Statement

==================================================================================
*/
/*
---------------------------------------------------------------------------------

	File Operations for Directory

---------------------------------------------------------------------------------
*/
static int me2fsReadDir( struct file *file, struct dir_context *ctx );
/*
---------------------------------------------------------------------------------

	Helper Functions for Directory

---------------------------------------------------------------------------------
*/
static inline unsigned long getDirNumPages( struct inode *inode );
static struct page*
me2fsGetDirPageCache( struct inode *inode, unsigned long index );
static inline void me2fsPutDirPageCache( struct page *page );
static unsigned long
me2fsGetPageLastByte( struct inode *inode, unsigned long page_nr );
static inline int
me2fsStrncmp( int len, const char* const name, struct ext2_dir_entry *dent );

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
const struct file_operations me2fs_dir_operations = 
{
	.iterate		= me2fsReadDir,
};


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
ino_t me2fsGetInoByName( struct inode *dir, struct qstr *child )
{
	struct ext2_dir_entry	*dent;
	struct page				*page;
	ino_t					ino;

	dent = me2fsFindDirEntry( dir, child, &page );

	if( dent )
	{
		ino = le32_to_cpu( dent->inode );
		me2fsPutDirPageCache( page );
	}
	else
	{
		ino = 0;
	}

	return( ino );
}
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
				   struct page **res_page )
{
	struct page				*page;
	struct ext2_dir_entry	*dent;
	unsigned long			rec_len;
	unsigned long			page_index;
	const char				*name = child->name;
	int						namelen;

	namelen	= child->len;
	rec_len	= ME2FS_DIR_REC_LEN( namelen );

	for( page_index = 0						;
		 page_index < getDirNumPages( dir )	;
		 page_index++ )
	{
		char	*start;
		char	*end;

		page = ( struct page* )me2fsGetDirPageCache( dir, page_index );

		if( IS_ERR( page ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:bad page [%lu\n]", __func__, page_index );
			goto not_found;
		}

		start	= ( char* )page_address( ( const struct page* )page );
		end		= start
				  + me2fsGetPageLastByte( dir, page_index )
				  - rec_len;
		dent	= ( struct ext2_dir_entry* )start;

		while( ( char* )dent <= end )
		{
			unsigned long	d_rec_len;

			if( dent->rec_len == 0 )
			{
				ME2FS_ERROR( "<ME2FS>%s:zero-length directoryentry\n", __func__ );
				me2fsPutDirPageCache( page );
				goto not_found;
			}

			if( me2fsStrncmp( namelen, name, dent ) )
			{
				goto found;
			}

			/* ---------------------------------------------------------------- */
			/* go to next entry													*/
			/* ---------------------------------------------------------------- */
			d_rec_len	= le16_to_cpu( dent->rec_len );

			dent = ( struct ext2_dir_entry* )( ( char* )dent + d_rec_len );
		}

		me2fsPutDirPageCache( page );
	}

not_found:
	return( NULL );

found:
	*res_page = page;
	return( dent );
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
---------------------------------------------------------------------------------

	File Operations for Directory

---------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:me2fsReadDir
	Input		:struct file *file
				 < vfs file object >
				 struct dir_context *ctx
				 < context of reading directory >
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
static int me2fsReadDir( struct file *file, struct dir_context *ctx )
{
	struct super_block		*sb;
	struct inode			*inode;
	unsigned long			offset;
	unsigned long			page_index;
	unsigned char			ftype_table[ EXT2_FT_MAX ] =
	{
		[ EXT2_FT_UNKNOWN	]	= DT_UNKNOWN,
		[ EXT2_FT_REG_FILE	]	= DT_REG,
		[ EXT2_FT_DIR		]	= DT_DIR,
		[ EXT2_FT_CHRDEV	]	= DT_CHR,
		[ EXT2_FT_BLKDEV	]	= DT_BLK,
		[ EXT2_FT_FIFO		]	= DT_FIFO,
		[ EXT2_FT_SOCK		]	= DT_SOCK,
		[ EXT2_FT_SYMLINK	]	= DT_LNK,
	};

	inode = file_inode( file );

	/* ------------------------------------------------------------------------ */
	/* whether position exceeds last of mininum dir entry or not				*/
	/* ------------------------------------------------------------------------ */
	if( ( inode->i_size - ( 1 + 8 + 3 ) ) < ctx->pos )
	{
		/* there is no more entry in the directory								*/
		return( 0 );
	}

	sb		= inode->i_sb;
	offset	= ctx->pos & ~PAGE_CACHE_MASK;

	for( page_index = ctx->pos >> PAGE_CACHE_SHIFT	;
		 page_index < getDirNumPages( inode )		;
		 page_index++ )
	{
		struct page				*page;
		struct ext2_dir_entry	*dent;
		char					*start;
		char					*end;

		page = ( struct page* )me2fsGetDirPageCache( inode, page_index );

		if( IS_ERR( page ) )
		{
			ME2FS_ERROR( "<ME2FS>bad page in %lu\n", inode->i_ino );
			ctx->pos += PAGE_CACHE_SIZE - offset;
			return( PTR_ERR( page ) );
		}

		start	= ( char* )page_address( ( const struct page* )page );
		end		= start
				  + me2fsGetPageLastByte( inode, page_index )
				  - ( 1 + 8 + 3 );
		dent	= ( struct ext2_dir_entry* )( start + offset );

		while( ( char* )dent <= end )
		{
			unsigned long	rec_len;
			unsigned char	ftype_indx;

			if( !dent->rec_len )
			{
				ME2FS_ERROR( "<ME2FS>error:zero-length directory entry\n" );
				me2fsPutDirPageCache( page );
				return( -EIO );
			}

			if( dent->inode )
			{
				if( EXT2_FT_MAX < dent->file_type )
				{
					ftype_indx = DT_UNKNOWN;
				}
				else
				{
					ftype_indx = dent->file_type;
				}

				if( !( dir_emit( ctx, dent->name, dent->name_len,
								 le32_to_cpu( dent->inode ),
								 ftype_table[ ftype_indx ] ) ) )
				{
					break;
				}
			}

			/* ---------------------------------------------------------------- */
			/* go to next entry													*/
			/* ---------------------------------------------------------------- */
			rec_len	= le16_to_cpu( dent->rec_len );

			ctx->pos+= rec_len;
			dent	= ( struct ext2_dir_entry* )( ( char* )dent + rec_len );
		}

		me2fsPutDirPageCache( page );
	}

	return( 0 );
}
/*
---------------------------------------------------------------------------------

	Helper Functions for Directory

---------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:getDirNumPages
	Input		:struct inode *inode
				 < vfs inode of a directory >
	Output		:void
	Return		:unsigned long
				 < number of pages to be cached for directory >

	Description	:get number of pages to be cached for directory
==================================================================================
*/
static inline unsigned long getDirNumPages( struct inode *inode )
{
	return( ( inode->i_size + PAGE_CACHE_SIZE - 1 ) >> PAGE_CACHE_SHIFT );
}

/*
==================================================================================
	Function	:me2fsGetDirPageCache
	Input		:struct inode *inode
				 < vfs inode of directory >
				 unsigned long index
				 < index of page cache >
	Output		:void
	Return		:struct page*
				 < page to which logical block attributes >

	Description	:get page cache of the directory
==================================================================================
*/
static struct page*
me2fsGetDirPageCache( struct inode *inode, unsigned long index )
{
	struct page	*page;

	/* ------------------------------------------------------------------------ */
	/* read blocks from device and map them										*/
	/* ------------------------------------------------------------------------ */
	/* find get a page from mapping if there is not allocated then alloc to it	*/
	/* and fill it. if there is cached page then return it						*/
	page = read_mapping_page( inode->i_mapping, index, NULL );

	if( !IS_ERR( page ) )
	{
		kmap( page );
		/* Actually linux ext2 module verifies page at here */
		if( PageError( page ) )
		{
			me2fsPutDirPageCache( page );
			return( ERR_PTR( -EIO ) );
		}
	}

	return( page );
}

/*
==================================================================================
	Function	:me2fsPutDirPageCache
	Input		:struct page *page
				 < page to put >
	Output		:void
	Return		:void

	Description	:put page
==================================================================================
*/
static inline void me2fsPutDirPageCache( struct page *page )
{
	kunmap( page );
	page_cache_release( page );
}
/*
==================================================================================
	Function	:me2fsGetPageLastByte
	Input		:struct inode *inode
				 < vfs inode >
				 unsigned long page_nr
				 < page number >
	Output		:void
	Return		:unsigned long
				 < last byte in that page >

	Description	:return the offset int page `page_nr' of the last valid
				 byte in that page, plus one.
==================================================================================
*/
static unsigned long
me2fsGetPageLastByte( struct inode *inode, unsigned long page_nr )
{
	unsigned long	last_byte;

	last_byte = inode->i_size - ( page_nr << PAGE_CACHE_SHIFT );

	if( last_byte > PAGE_CACHE_SIZE )
	{
		return( PAGE_CACHE_SIZE );
	}

	return( last_byte );
}
/*
==================================================================================
	Function	:me2fsStrncmp
	Input		:int len
				 < length of name >
				 const char* const name
				 < name of comparison >
				 struct ext2_dir_entry *dent
				 < directory entry to be compared >
	Output		:void
	Return		:void

	Description	:compare name and name of direcotry entry
				 < 0 : mismatch, 1 : match >
==================================================================================
*/
static inline int
me2fsStrncmp( int len, const char* const name, struct ext2_dir_entry *dent )
{
	if( len != dent->name_len )
	{
		return( 0 );
	}
	
	if( !dent->inode )
	{
		return( 0 );
	}

	return( !memcmp( name, dent->name, len ) );
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
