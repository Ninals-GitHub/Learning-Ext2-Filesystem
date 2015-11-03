/********************************************************************************
	File			: me2fs_dir.c
	Description		: Directory Operations of my ext2 file system

*********************************************************************************/
#include <linux/buffer_head.h>
#include <linux/pagemap.h>
#include <linux/swap.h>

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
	struct me2fs_inode_info	*mei;
	struct ext2_dir_entry	*dent;
	struct buffer_head		*bh;
	unsigned long			offset;
	unsigned long			block_index;
	unsigned long			read_len;
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

	sb = inode->i_sb;

	/* calculate offset in directory page caches								*/
	block_index = ctx->pos >> sb->s_blocksize_bits;
	offset		= ctx->pos & ~( ( 1 << sb->s_blocksize_bits ) - 1 );

	bh = NULL;

	if( block_index < ME2FS_IND_BLOCK )
	{
		mei = ME2FS_I( inode );
		if( !( bh = sb_bread( sb, mei->i_data[ block_index ] ) ) )
		{
			ME2FS_ERROR( "<ME2FS>cannot read block %d\n",
						 mei->i_data[ block_index ] );
			return( -1 );
		}

	}
	else if( offset < ME2FS_2IND_BLOCK )
	{
		DBGPRINT( "<ME2FS>UNIMPLEMENTED INDIRECT BLOCK!\n" );
		return( -1 );
	}

	DBGPRINT( "<ME2FS>FILL Dir entries!\n" );

	dent = ( struct ext2_dir_entry* )( bh->b_data + offset );

	read_len = 1 + 8 + 3;
	DBGPRINT( "<ME2FS>inode size = %llu\n",( unsigned long long )inode->i_size );

	for( ; ; )
	{
		if( !dent->rec_len )
		{
			DBGPRINT( "<ME2FS>Fill dir is over!\n" );
			break;
		}
		if( ( ( inode->i_size - ( 1 + 8 + 3 ) ) < ctx->pos ) ||
			( sb->s_blocksize <= ( read_len - ( 1 + 8 + 3 ) ) ) )
		{
			DBGPRINT( "<ME2FS>Fill dir is over!\n" );
			break;
		}

		if( dent->inode )
		{
			unsigned char	ftype_indx;
			int				i;
			DBGPRINT( "<ME2FS>[fill dirent]inode=%u\n",
					  le32_to_cpu( dent->inode ) );
			DBGPRINT( "<ME2FS>[fill dirent]name_len=%d\n", dent->name_len );
			DBGPRINT( "<ME2FS>[fill dirent]rec_len=%u\n",
					  le16_to_cpu( dent->rec_len ) );
			DBGPRINT( "<ME2FS>[fill dirent]read_len=%lu\n", read_len );
			DBGPRINT( "<ME2FS>[fill dirent]ctx->pos=%llu\n",
					  ( unsigned long long )ctx->pos );
			DBGPRINT( "<ME2FS>[fill dirent]name =\"" );
			for( i = 0 ; i < dent->name_len ; i++ )
			{
				DBGPRINT( "%c", dent->name[ i ] );
			}
			DBGPRINT( "\"\n" );
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

			dent = ( struct ext2_dir_entry* )( ( unsigned char* )dent + dent->rec_len );
		}


		ctx->pos	+= le16_to_cpu( dent->rec_len );
		read_len	+= le16_to_cpu( dent->rec_len );
	}

	brelse( bh );
	
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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
