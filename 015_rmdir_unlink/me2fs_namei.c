/********************************************************************************
	File			: me2fs_namei.c
	Description		: name space operations of my ext2 file system

*********************************************************************************/
#include <linux/pagemap.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"
#include "me2fs_dir.h"
#include "me2fs_ialloc.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
/*
----------------------------------------------------------------------------------

	Inode Operations for Directory

----------------------------------------------------------------------------------
*/
static struct dentry*
me2fsLookup( struct inode *dir, struct dentry *dentry, unsigned int flags );
static int
me2fsMkdir( struct inode *dir, struct dentry *dentry, umode_t mode );
static int
me2fsUnlink( struct inode *dir, struct dentry *dentry );
static int
me2fsRmdir( struct inode *dir, struct dentry *dentry );

/*
----------------------------------------------------------------------------------

	Helper Functions

----------------------------------------------------------------------------------
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
static int me2fsCreate( struct inode *inode, struct dentry *dir,
						umode_t mode, bool flag )
{ DBGPRINT( "<ME2FS>inode ops:create!\n" ); return 0; }
static int me2fsLink( struct dentry *dentry, struct inode *inode, struct dentry *d)
{ DBGPRINT( "<ME2FS>inode ops:link!\n" ); return 0; }
//static int me2fsUnlink( struct inode *inode, struct dentry *d )
//{ DBGPRINT( "<ME2FS>inode ops:unlink!\n" ); return 0; }
static int me2fsSymlink( struct inode *inode, struct dentry *d, const char *name )
{ DBGPRINT( "<ME2FS>inode ops:symlink!\n" ); return 0; }
//static int me2fsMkdir( struct inode *inode, struct dentry *d, umode_t mode )
//{ DBGPRINT( "<ME2FS>inode ops:mkdir!\n" ); return 0; }
//static int me2fsRmdir( struct inode *inode, struct dentry *d )
//{ DBGPRINT( "<ME2FS>inode ops:rmdir!\n" ); return 0; }
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
	struct inode	*inode;
	ino_t			ino;

	DBGPRINT( "<ME2FS>LOOKUP %s!\n", dentry->d_name.name );

	if( ME2FS_NAME_LEN < dentry->d_name.len )
	{
		return( ERR_PTR( -ENAMETOOLONG ) );
	}
	

	ino = me2fsGetInoByName( dir, &dentry->d_name );

	inode = NULL;

	DBGPRINT( "<ME2FS>LOOKUP ino=%lu\n", ino );

	if( ino )
	{
		inode = me2fsGetVfsInode( dir->i_sb, ino );

		if( inode == ERR_PTR( -ESTALE ) )
		{
			ME2FS_ERROR( "<ME2FS>%s:deleted inode referenced\n", __func__ );
			return( ERR_PTR( -EIO ) );
		}
	}

	return( d_splice_alias( inode, dentry ) );
}
/*
==================================================================================
	Function	:me2fsMkdir
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct dentry *dentry
				 < dentry to be made its entry >
				 umode_t mode
				 < file mode >
	Output		:void
	Return		:int
				 < result >

	Description	:make a directory in *dir
==================================================================================
*/
static int
me2fsMkdir( struct inode *dir, struct dentry *dentry, umode_t mode )
{
	struct inode	*inode;
	int				err;

#if 0	// quota
	dquot_initialize( dir );
#endif

	DBGPRINT( "<ME2FS>mkdir : start make [%s]\n", dentry->d_name.name );

	/* ------------------------------------------------------------------------ */
	/* allocate a new inode for new directory									*/ 
	/* ------------------------------------------------------------------------ */
	inode_inc_link_count( dir );
	inode = me2fsAllocNewInode( dir, S_IFDIR | mode, &dentry->d_name );

	if( IS_ERR( inode ) )
	{
		inode_dec_link_count( dir );
		return( PTR_ERR( inode ) );
	}

	inode->i_op				= &me2fs_dir_inode_operations;
	inode->i_fop			= &me2fs_dir_operations;
	inode->i_mapping->a_ops	= &me2fs_aops;

	/* ------------------------------------------------------------------------ */
	/* make empty directory( just make '.')										*/
	/* ------------------------------------------------------------------------ */
	inode_inc_link_count( inode );

	if( ( err = me2fsMakeEmpty( inode, dir ) ) )
	{
		goto out_fail;
	}

	/* ------------------------------------------------------------------------ */
	/* insert the new directory's inode to its parent							*/
	/* ------------------------------------------------------------------------ */
	if( ( err = me2fsAddLink( dentry, inode ) ) )
	{
		goto out_fail;
	}

	unlock_new_inode( inode );
	d_instantiate( dentry, inode );

	DBGPRINT( "<ME2FS>mkdir : complete [%s]\n", dentry->d_name.name );

	return( err );

out_fail:
	DBGPRINT( "<ME2FS>failed to make dir\n" );
	inode_dec_link_count( inode );
	inode_dec_link_count( inode );
	unlock_new_inode( inode );
	iput( inode );

	return( err );
}

/*
==================================================================================
	Function	:me2fsRmdir
	Input		:struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < name of directory >
	Output		:void
	Return		:int
				 < result >

	Description	:remove directory
==================================================================================
*/
static int
me2fsRmdir( struct inode *dir, struct dentry *dentry )
{
	struct inode	*inode;
	int				err;

	err		= -ENOTEMPTY;
	inode	= dentry->d_inode;

	if( me2fsIsEmptyDir( inode ) )
	{
		if( !( err = me2fsUnlink( dir, dentry ) ) )
		{
			inode->i_size = 0;
			inode_dec_link_count( inode );
			inode_dec_link_count( dir );
		}
	}

	return( err );
}

/*
==================================================================================
	Function	:me2fsUnlink
	Input		:struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < name of file >
	Output		:void
	Return		:int
				 < result >

	Description	:unlink a file
==================================================================================
*/
static int
me2fsUnlink( struct inode *dir, struct dentry *dentry )
{
	struct inode			*inode;
	struct ext2_dir_entry	*dent;
	struct page				*page;
	int						err;

	if( !( dent = me2fsFindDirEntry( dir, &dentry->d_name, &page ) ) )
	{
		return( -ENOENT );
	}

	if( ( err = me2fsDeleteDirEntry( dent, page ) ) )
	{
		return( err );
	}

	inode			= dentry->d_inode;
	inode->i_ctime	= dir->i_ctime;
	inode_dec_link_count( inode );

	return( 0 );


}
/*
----------------------------------------------------------------------------------

	Helper Functions

----------------------------------------------------------------------------------
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
