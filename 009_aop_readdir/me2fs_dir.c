/********************************************************************************
	File			: me2fs_dir.c
	Description		: Directory Operations of my ext2 file system

*********************************************************************************/

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"



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
----------------------------------------------------------------------------------

	Inode Operations for Directory

----------------------------------------------------------------------------------
*/
static struct dentry*
me2fsLookup( struct inode *dir, struct dentry *dentry, unsigned int flags );
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

static int me2fsCreate( struct inode *inode, struct dentry *dir,
						umode_t mode, bool flag )
{ DBGPRINT( "<ME2FS>inode ops:create!\n" ); return 0; }
static int me2fsLink( struct dentry *dentry, struct inode *inode, struct dentry *d)
{ DBGPRINT( "<ME2FS>inode ops:link!\n" ); return 0; }
static int me2fsUnlink( struct inode *inode, struct dentry *d )
{ DBGPRINT( "<ME2FS>inode ops:unlink!\n" ); return 0; }
static int me2fsSymlink( struct inode *inode, struct dentry *d, const char *name )
{ DBGPRINT( "<ME2FS>inode ops:symlink!\n" ); return 0; }
static int me2fsMkdir( struct inode *inode, struct dentry *d, umode_t mode )
{ DBGPRINT( "<ME2FS>inode ops:mkdir!\n" ); return 0; }
static int me2fsRmdir( struct inode *inode, struct dentry *d )
{ DBGPRINT( "<ME2FS>inode ops:rmdir!\n" ); return 0; }
static int me2fsMknod( struct inode *inode, struct dentry *d,
					umode_t mode, dev_t dev )
{ DBGPRINT( "<ME2FS>inode ops:mknod!\n" ); return 0; }
static int me2fsRename( struct inode *inode, struct dentry *d,
					struct inode *inode2, struct dentry *d2 )
{ DBGPRINT( "<ME2FS>inode ops:rename!\n" ); return 0; }
static int me2fsSetAttr( struct dentry *dentry, struct iattr *attr )
{ DBGPRINT( "<ME2FS>inode ops:setattr!\n" ); return 0; }
static struct posix_acl* me2fsGetAcl( struct inode *inode, int flags )
{ DBGPRINT( "<ME2FS>inode ops:get_acl!\n" ); return 0; }
static int me2fsTmpFile( struct inode *inode, struct dentry *d, umode_t mode )
{ DBGPRINT( "<ME2FS>inode ops:tmpfile!\n" ); return 0; }


const struct inode_operations me2fs_dir_inode_operations =
{
	.create			= me2fsCreate,
	.lookup			= me2fsLookup,
	.link			= me2fsLink,
	.unlink			= me2fsUnlink,
	.symlink		= me2fsSymlink,
	.mkdir			= me2fsMkdir,
	.rmdir			= me2fsRmdir,
	.mknod			= me2fsMknod,
	.rename			= me2fsRename,
	.setattr		= me2fsSetAttr,
	.get_acl		= me2fsGetAcl,
	.tmpfile		= me2fsTmpFile,
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
----------------------------------------------------------------------------------

	Inode Operations for Directory

----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:me2fsLookup
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct dentry *dentry
				 < dentry to be looked up >
				 unsigned int flags
				 < look up flags >
	Output		:void
	Return		:struct dentry*
				 < looked up dentry >

	Description	:look up dentry in directory
==================================================================================
*/
static struct dentry*
me2fsLookup( struct inode *dir, struct dentry *dentry, unsigned int flags )
{
	DBGPRINT( "<ME2FS>LOOKUP %s!\n", dentry->d_name.name );
	return( NULL );
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
	DBGPRINT( "page cache inode = %lu\n", ( unsigned long )inode->i_ino );
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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
