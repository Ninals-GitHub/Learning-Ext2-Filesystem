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
	if( ctx->pos == 0 )
	{
		DBGPRINT( "<ME2FS>Read Dir dot !\n" );
		if( !dir_emit_dot( file, ctx ) )
		{
			ME2FS_ERROR( "<ME2FS>cannot emit \".\" directory entry\n" );
			return( -1 );
		}
		ctx->pos = 1;
	}
	if( ctx->pos == 1 )
	{
		DBGPRINT( "<ME2FS>Read Dir dot dot !\n" );
		if( !dir_emit_dotdot( file,ctx ) )
		{
			ME2FS_ERROR( "<ME2FS>cannot emit \"..\" directory entry\n" );
			return( -1 );
		}
		ctx->pos = 2;
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
	DBGPRINT( "<ME2FS>LOOKUP in directory!\n" );
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
