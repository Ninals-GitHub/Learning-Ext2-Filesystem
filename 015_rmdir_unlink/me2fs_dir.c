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
static inline void
setDirEntryType( struct ext2_dir_entry *dent, struct inode *inode );
static inline int
prepareWriteBlock( struct page *page, loff_t pos, unsigned long len );
static int commitBlockWrite( struct page *page, loff_t pos, unsigned long len );

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

//#define	S_SHIFT		16
#define	S_SHIFT		12
static unsigned char me2fs_type_by_mode[ S_IFMT >> S_SHIFT ] =
{
	[ S_IFREG	>> S_SHIFT	]	= EXT2_FT_REG_FILE,
	[ S_IFDIR	>> S_SHIFT	]	= EXT2_FT_DIR,
	[ S_IFCHR	>> S_SHIFT	]	= EXT2_FT_CHRDEV,
	[ S_IFBLK	>> S_SHIFT	]	= EXT2_FT_BLKDEV,
	[ S_IFIFO	>> S_SHIFT	]	= EXT2_FT_FIFO,
	[ S_IFSOCK	>> S_SHIFT	]	= EXT2_FT_SOCK,
	[ S_IFLNK	>> S_SHIFT	]	= EXT2_FT_SYMLINK,

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

	for( page_index = 0							;
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
	Function	:me2fsMakeEmpty
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *parent
				 < parent inode >
	Output		:void
	Return		:int
				 < result >

	Description	:make empty entry
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsMakeEmpty( struct inode *inode, struct inode *parent )
{
	struct page				*page;
	struct ext2_dir_entry	*dent;
	unsigned long			block_size;
	int						err;
	void					*start;

	/* -------------------------------------------------------------------------*/
	/* find or create a page for index 0										*/
	/* -------------------------------------------------------------------------*/
	if( !( page = grab_cache_page( inode->i_mapping, 0 ) ) )
	{
		return( -ENOMEM );
	}

	/* get block size in file system											*/
	block_size = inode->i_sb->s_blocksize;

	if( ( err = prepareWriteBlock( page, 0, block_size ) ) )
	{
		/* failed to prepare													*/
		unlock_page( page );
		page_cache_release( page );
		return( err );
	}

	start = kmap_atomic( page );

	memset( start, 0, block_size );

	/* -------------------------------------------------------------------------*/
	/* make dot																	*/
	/* -------------------------------------------------------------------------*/
	dent = ( struct ext2_dir_entry* )start;
	dent->name_len	= 1;
	dent->rec_len	= cpu_to_le16( ME2FS_DIR_REC_LEN( dent->name_len ) );
	memcpy( dent->name, ".\0\0", 4 );
	dent->inode = cpu_to_le32( inode->i_ino );
	setDirEntryType( dent, inode );

	/* -------------------------------------------------------------------------*/
	/* make dot dot																*/
	/* -------------------------------------------------------------------------*/
	dent = ( struct ext2_dir_entry* )( start + ME2FS_DIR_REC_LEN( 1 ) );
	dent->name_len	= 2;
	dent->rec_len	= cpu_to_le16( block_size - ME2FS_DIR_REC_LEN( 1 ) );
	dent->inode		= cpu_to_le32( parent->i_ino );
	memcpy( dent->name, "..\0", 4 );
	setDirEntryType( dent, inode );


	kunmap_atomic( start );

	/* -------------------------------------------------------------------------*/
	/* commit write block of empty contents										*/
	/* -------------------------------------------------------------------------*/
	err = commitBlockWrite( page, 0, block_size );

	page_cache_release( page );

	return( err );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsAddLink
	Input		:struct dentry *dentry
				 < vfs dentry >
				 struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:add link
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsAddLink( struct dentry *dentry, struct inode *inode )
{
	struct inode			*dir;
	struct page				*page;
	struct ext2_dir_entry	*dent;
	const char				*link_name = dentry->d_name.name;
	int						link_name_len;
	unsigned long			block_size;
	unsigned long			link_rec_len;
	unsigned short			rec_len;
	unsigned short			name_len;
	unsigned long			page_index;
	loff_t					pos;
	int						err;

	dir				= dentry->d_parent->d_inode;
	link_name_len	= dentry->d_name.len;
	link_rec_len	= ME2FS_DIR_REC_LEN( link_name_len );

	block_size		= dir->i_sb->s_blocksize;

	/* ------------------------------------------------------------------------ */
	/* find entry space in the directory										*/
	/* ------------------------------------------------------------------------ */
	for( page_index = 0							;
		 page_index <= getDirNumPages( dir )	;
		 page_index++ )
	{
		char	*start;
		char	*end;
		char	*dir_end;

		page = ( struct page* )me2fsGetDirPageCache( dir, page_index );

		err = PTR_ERR( page );

		if( IS_ERR( page ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:bad page [%lu\n]", __func__, page_index );
			return( err );
		}
		
		lock_page( page );
		start	= ( char* )page_address( ( const struct page* )page );
		dir_end	= start
				  + me2fsGetPageLastByte( dir, page_index );
		dent	= ( struct ext2_dir_entry* )start;
		
		end		= start + PAGE_CACHE_SIZE - link_rec_len;

		/* -------------------------------------------------------------------- */
		/* find entry space in the page cache of the directory					*/
		/* -------------------------------------------------------------------- */
		while( ( char* )dent <= end )
		{
			if( ( char* )dent == dir_end )
			{
				/* ------------------------------------------------------------ */
				/* reach i_size													*/
				/* ------------------------------------------------------------ */
				name_len		= 0;
				rec_len			= block_size;
				dent->rec_len	= cpu_to_le16( rec_len );
				dent->inode		= 0;
				
				goto got_it;
			}

			/* ---------------------------------------------------------------- */
			/* invalid entry													*/
			/* ---------------------------------------------------------------- */
			if( !dent->rec_len )
			{
				ME2FS_ERROR( "<ME2FS>%s:zero-length directory entry\n", __func__ );
				err = -EIO;
				goto out_unlock;
			}

			/* ---------------------------------------------------------------- */
			/* the entry already exists											*/
			/* ---------------------------------------------------------------- */
			if( me2fsStrncmp( link_name_len, link_name, dent ) )
			{
				err = -EEXIST;
				goto out_unlock;
			}

			name_len	= ME2FS_DIR_REC_LEN( dent->name_len );
			rec_len		= le16_to_cpu( dent->rec_len );

			/* ---------------------------------------------------------------- */
			/* found empty entry												*/
			/* ---------------------------------------------------------------- */
			if( !dent->inode && ( link_rec_len <= rec_len ) )
			{
				goto got_it;
			}

			/* ---------------------------------------------------------------- */
			/* detected deteleted entry											*/
			/* ---------------------------------------------------------------- */
			if( ( name_len + link_rec_len ) <= rec_len )
			{
				goto got_it;
			}

			dent = ( struct ext2_dir_entry* )( ( char* )dent + rec_len );
		}

		unlock_page( page );
		me2fsPutDirPageCache( page );
	}

	return( -EINVAL );

got_it:
	pos = page_offset( page )
		  + ( ( char* )dent - ( char* )page_address( page ) );
	
	if( ( err = prepareWriteBlock( page, pos, rec_len ) ) )
	{
		goto out_unlock;
	}

	/* ------------------------------------------------------------------------ */
	/* insert deteleted entry space												*/
	/* ------------------------------------------------------------------------ */
	if( dent->inode )
	{
		struct ext2_dir_entry	*cur_dent;
		/* here, name_len = rec_len of deleted entry							*/
		cur_dent = ( struct ext2_dir_entry* )( ( char* )dent + name_len );
		cur_dent->rec_len	= cpu_to_le16( rec_len - name_len );
		dent->rec_len		= cpu_to_le16( name_len );
		dent				= cur_dent;
	}

	/* ------------------------------------------------------------------------ */
	/* finally link the inode to parent directory								*/
	/* ------------------------------------------------------------------------ */
	dent->name_len	= link_name_len;
	memcpy( dent->name, link_name, link_name_len );
	dent->inode		= cpu_to_le32( inode->i_ino );
	setDirEntryType( dent, inode );

	err = commitBlockWrite( page, pos, rec_len );
	dir->i_mtime = CURRENT_TIME_SEC;
	dir->i_ctime = dir->i_mtime;
	ME2FS_I( dir )->i_flags &= ~EXT2_BTREE_FL;
	mark_inode_dirty( dir );
	
	me2fsPutDirPageCache( page );

	return( err );


out_unlock:
	unlock_page( page );
	me2fsPutDirPageCache( page );
	return( err );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsIsEmptyDir
	Input		:struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:int
				 < 0:false 1:true >

	Description	:check if the directory is empty or not
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsIsEmptyDir( struct inode *inode )
{
	struct page		*page;
	unsigned long	i;

	page			= NULL;

	for( i = 0 ; i < getDirNumPages( inode ) ; i++ )
	{
		char					*start;
		char					*end;
		struct ext2_dir_entry	*dent;

		page = me2fsGetDirPageCache( inode, i );

		if( IS_ERR( page ) )
		{
			continue;
		}

		start	= page_address( page );
		dent	= ( struct ext2_dir_entry* )start;

		end		= start + ( me2fsGetPageLastByte( inode, i )
							- ME2FS_DIR_REC_LEN( 1 ) );

		while( ( char* )dent <= end )
		{
			if( dent->rec_len == 0 )
			{
				ME2FS_ERROR( "<ME2FS>%s:zero-length directory entry\n",
							 __func__ );
			}

			if( dent->inode != 0 )
			{
				if( dent->name[ 0 ] != '.' )
				{
					goto not_empty;
				}
				if( 2 < dent->name_len )
				{
					goto not_empty;
				}
				if( dent->name_len < 2 )
				{
					if( dent->inode != cpu_to_le32( inode->i_ino ) )
					{
						goto not_empty;
					}
				}
				else if( dent->name[ 1 ] != '.' )
				{
					goto not_empty;
				}

				/* goto next entry												*/
				dent = ( struct ext2_dir_entry* )
							( ( char* )dent + le16_to_cpu( dent->rec_len ) );
			}

			me2fsPutDirPageCache( page );
		}
	}

	return( 1 );

not_empty:
	me2fsPutDirPageCache( page );
	return( 0 );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsDeleteDirEntry
	Input		:struct ext2_dir_entry *dir
				 < directory entry to delete >
				 struct page *page
				 < page cache the directory belongs to >
	Output		:void
	Return		:void

	Description	:delete a directory entry
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsDeleteDirEntry( struct ext2_dir_entry *dir, struct page *page )
{
	struct inode			*inode;
	char					*start;
	unsigned				from;
	unsigned				to;
	loff_t					pos;
	struct ext2_dir_entry	*pde;
	struct ext2_dir_entry	*dent;
	int						err;

	inode	= page->mapping->host;
	start	= page_address( page );
	from	= ( ( char* )dir - start ) & ~( inode->i_sb->s_blocksize - 1 );
	to		= ( ( char* )dir - start ) + le16_to_cpu( dir->rec_len );

	pde		= NULL;
	dent	= ( struct ext2_dir_entry* )( start + from );

	while( ( char* )dent < ( char* )dir )
	{
		if( dent->rec_len == 0 )
		{
			ME2FS_ERROR( "<ME2FS>%s:zero-length directory entry\n",
						 __func__ );
			err = -EIO;
			goto out;
		}

		pde = dent;
		dent = ( struct ext2_dir_entry* )( ( char* )dent
										   + le16_to_cpu( dent->rec_len ) );
	}

	if( pde )
	{
		from = ( char* )pde - start;
	}

	pos = page_offset( page ) + from;
	lock_page( page );
	err = prepareWriteBlock( page, pos, to - from );
	if( pde )
	{
		pde->rec_len = le16_to_cpu( to - from );
	}
	dir->inode					= 0;
	err							= commitBlockWrite( page, pos, to - from );
	inode->i_mtime				= CURRENT_TIME_SEC;
	inode->i_ctime				= inode->i_mtime;
	ME2FS_I( inode )->i_flags	&= ~EXT2_BTREE_FL;

	mark_inode_dirty( inode );

out:
	me2fsPutDirPageCache( page );
	return( err );
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
	Function	:setDirEntryType
	Input		:struct ext2_dir_entry *dent
				 < directory entry >
				 struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:void

	Description	:set file type of vfs to ext2 directory entry
==================================================================================
*/
static inline void
setDirEntryType( struct ext2_dir_entry *dent, struct inode *inode )
{
	struct me2fs_sb_info	*msi;
	umode_t					mode;

	mode	= inode->i_mode;
	msi		= ME2FS_SB( inode->i_sb );

	if( msi->s_esb->s_feature_incompat
		& cpu_to_le32( EXT2_FEATURE_INCOMPAT_FILETYPE ) )
	{
		dent->file_type = me2fs_type_by_mode[ ( mode & S_IFMT ) >> S_SHIFT ];
	}
	else
	{
		dent->file_type = 0;
	}
}

/*
==================================================================================
	Function	:prepareWriteBlock
	Input		:struct page *page
				 < page of file >
				 loff_t pos
				 < position in file >
				 unsigned long len
				 < length of write data >
	Output		:void
	Return		:int
				 < result >

	Description	:prepare to write a block
==================================================================================
*/
static inline int
prepareWriteBlock( struct page *page, loff_t pos, unsigned long len )
{
	return( __block_write_begin( page, pos, ( unsigned )len, me2fsGetBlock ) );
}

/*
==================================================================================
	Function	:commitBlockWrite
	Input		:struct page *page
				 < page of file >
				 loff_t pos
				 < offset in file >
				 unsigned long len
				 < length to commit >
	Output		:void
	Return		:int
				 < result >

	Description	:commit block write
==================================================================================
*/
static int commitBlockWrite( struct page *page, loff_t pos, unsigned long len )
{
	struct address_space	*mapping;
	struct inode			*dir;
	int						err;

	mapping	= page->mapping;
	dir		= mapping->host;
	err		= 0;

	/* ------------------------------------------------------------------------ */
	/* commit block write														*/
	/* ------------------------------------------------------------------------ */
	block_write_end( NULL, mapping, pos, len, len, page, NULL );

	if( dir->i_size < ( pos + len ) )
	{
		i_size_write( dir, pos + len );
		mark_inode_dirty( dir );
	}

	/* ------------------------------------------------------------------------ */
	/* sync file and inode														*/
	/* ------------------------------------------------------------------------ */
	if( IS_DIRSYNC( dir ) )
	{
		if( !( err = write_one_page( page, 1 ) ) )
		{
			err = sync_inode_metadata( dir, 1 );
		}
	}
	else
	{
		unlock_page( page );
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
