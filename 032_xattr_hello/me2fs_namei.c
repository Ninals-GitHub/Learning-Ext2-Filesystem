/********************************************************************************
	File			: me2fs_namei.c
	Description		: name space operations of my ext2 file system

*********************************************************************************/
#include <linux/pagemap.h>
#include <linux/xattr.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"
#include "me2fs_dir.h"
#include "me2fs_ialloc.h"
#include "me2fs_file.h"
#include "me2fs_symlink.h"
#include "me2fs_ioctl.h"
#include "me2fs_xattr.h"


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
static int
me2fsRename( struct inode *old_dir,
			 struct dentry *old_dentry,
			 struct inode *new_dir,
			 struct dentry *new_dentry );
static int me2fsCreate( struct inode *dir,
						struct dentry *dentry,
						umode_t mode,
						bool excl );
static int
me2fsLink( struct dentry *old_dentry, struct inode *dir, struct dentry *dentry );
static int
me2fsSymlink( struct inode *dir,
			  struct dentry *dentry,
			  const char *symname );
static int
me2fsMknod( struct inode *dir,
			struct dentry *dentry,
			umode_t mode,
			dev_t rdev );
static int
me2fsTmpFile( struct inode *dir,
			  struct dentry *dentry,
			  umode_t mode );

/*
----------------------------------------------------------------------------------

	Helper Functions

----------------------------------------------------------------------------------
*/
static inline int
addNonDir( struct dentry *dentry, struct inode *inode );

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
//static int me2fsCreate( struct inode *inode, struct dentry *dir,
//						umode_t mode, bool flag )
//{ DBGPRINT( "<ME2FS>inode ops:create!\n" ); return 0; }
//static int me2fsLink( struct dentry *dentry, struct inode *inode, struct dentry *d)
//{ DBGPRINT( "<ME2FS>inode ops:link!\n" ); return 0; }
//static int me2fsUnlink( struct inode *inode, struct dentry *d )
//{ DBGPRINT( "<ME2FS>inode ops:unlink!\n" ); return 0; }
//static int me2fsSymlink( struct inode *inode, struct dentry *d, const char *name )
//{ DBGPRINT( "<ME2FS>inode ops:symlink!\n" ); return 0; }
//static int me2fsMkdir( struct inode *inode, struct dentry *d, umode_t mode )
//{ DBGPRINT( "<ME2FS>inode ops:mkdir!\n" ); return 0; }
//static int me2fsRmdir( struct inode *inode, struct dentry *d )
//{ DBGPRINT( "<ME2FS>inode ops:rmdir!\n" ); return 0; }
//static int me2fsMknod( struct inode *inode, struct dentry *d,
//					umode_t mode, dev_t dev )
//{ DBGPRINT( "<ME2FS>inode ops:mknod!\n" ); return 0; }
//static int me2fsRename( struct inode *inode, struct dentry *d,
//					struct inode *inode2, struct dentry *d2 )
//{ DBGPRINT( "<ME2FS>inode ops:rename!\n" ); return 0; }
//static int me2fsSetAttr( struct dentry *dentry, struct iattr *attr )
//{ DBGPRINT( "<ME2FS>inode ops:setattr!\n" ); return 0; }
static struct posix_acl* me2fsGetAcl( struct inode *inode, int flags )
{ DBGPRINT( "<ME2FS>inode ops:get_acl!\n" ); return 0; }
//static int me2fsTmpFile( struct inode *inode, struct dentry *d, umode_t mode )
//{ DBGPRINT( "<ME2FS>inode ops:tmpfile!\n" ); return 0; }


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

	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.listxattr		= me2fsListXattr,
	.removexattr	= generic_removexattr,
};

const struct inode_operations me2fs_special_inode_operations =
{
	.setattr		= me2fsSetAttr,
	.get_acl		= me2fsGetAcl,

	.setxattr		= generic_setxattr,
	.getxattr		= generic_getxattr,
	.listxattr		= me2fsListXattr,
	.removexattr	= generic_removexattr,
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
==================================================================================
	Function	:me2fsRename
	Input		:struct inode *old_dir
				 < vfs inode of old parent directory >
				 struct dentry *old_dentry
				 < old file name >
				 struct inode *new_dir
				 < vfs inode of new parent directory >
				 struct dentry *new_dentry
				 < new file name >
	Output		:void
	Return		:int
				 < result >

	Description	:rename old_dir/old_file_name to new_dir/new_file_name
==================================================================================
*/
static int
me2fsRename( struct inode *old_dir,
			 struct dentry *old_dentry,
			 struct inode *new_dir,
			 struct dentry *new_dentry )
{
	struct inode			*old_inode;
	struct page				*old_page;
	struct ext2_dir_entry	*old_dent;

	struct inode			*new_inode;

	struct page				*dir_page;
	struct ext2_dir_entry	*dir_dent;

	int						err;

	old_inode	= old_dentry->d_inode;
	old_dent	= me2fsFindDirEntry( old_dir,
									 &old_dentry->d_name,
									 &old_page );
	
	new_inode	= new_dentry->d_inode;

	dir_page	= NULL;
	dir_dent	= NULL;

	err = -EIO;
	
	if( !old_dent )
	{
		goto out;
	}

	if( S_ISDIR( old_inode->i_mode ) )
	{
		dir_dent = me2fsGetDotDotEntry( old_inode, &dir_page );
		if( !dir_dent )
		{
			goto out_old;
		}
	}

	if( new_inode )
	{
		struct page				*new_page;
		struct ext2_dir_entry	*new_dent;

		if( dir_dent && !me2fsIsEmptyDir( new_inode ) )
		{
			err = -ENOTEMPTY;
			goto out_dir;
		}

		new_dent = me2fsFindDirEntry( new_dir, &new_dentry->d_name, &new_page );
		if( !new_dent )
		{
			err = -ENOENT;
			goto out_dir;
		}

		me2fsSetLink( new_dir, new_dent, new_page, old_inode, 1 );

		new_inode->i_ctime = CURRENT_TIME_SEC;

		if( dir_dent )
		{
			drop_nlink( new_inode );
		}

		inode_dec_link_count( new_inode );
	}
	else
	{
		if( ( err = me2fsAddLink( new_dentry, old_inode ) ) )
		{
			goto out_dir;
		}

		if( dir_dent )
		{
			inode_inc_link_count( new_dir );
		}
	}

	old_inode->i_ctime = CURRENT_TIME_SEC;
	mark_inode_dirty( old_inode );

	me2fsDeleteDirEntry( old_dent, old_page );

	if( dir_dent )
	{
		if( old_dir != new_dir )
		{
			me2fsSetLink( old_inode, dir_dent, dir_page, new_dir, 0 );
		}
		else
		{
			kunmap( dir_page );
			page_cache_release( dir_page );
		}

		inode_dec_link_count( old_dir );
	}

	return( 0 );

out_dir:
	if( dir_dent )
	{
		kunmap( dir_page );
		page_cache_release( dir_page );
	}

out_old:
	kunmap( old_page );
	page_cache_release( old_page );

out:
	return( err );
}

/*
==================================================================================
	Function	:me2fsCreate
	Input		:struct inode *dir
				 < vfs inode of directory >
				 struct dentry *dentry
				 < name of file to create >
				 umode_t mode
				 < file mode >
				 bool excl
				 < O_EXCL flag >
	Output		:void
	Return		:int
				 < result >

	Description	:create a file
==================================================================================
*/
static int me2fsCreate( struct inode *dir,
						struct dentry *dentry,
						umode_t mode,
						bool excl )
{
	struct inode	*inode;

	DBGPRINT( "<ME2FS>%s:create [%s]\n", __func__, dentry->d_name.name );

	inode = me2fsAllocNewInode( dir, mode, &dentry->d_name );

	if( IS_ERR( inode ) )
	{
		DBGPRINT( "<ME2FS>%s:failed to create[%s]\n", __func__, dentry->d_name.name );
		return( PTR_ERR( inode ) );
	}

	inode->i_op				= &me2fs_file_inode_operations;
	inode->i_mapping->a_ops	= &me2fs_aops;
	inode->i_fop			= &me2fs_file_operations;

	mark_inode_dirty( inode );

	return( addNonDir( dentry, inode ) );
}

/*
==================================================================================
	Function	:me2fsLink
	Input		:struct dentry *old_dentry
				 < old file name >
				 struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < new file name >
	Output		:void
	Return		:int
				 < result >

	Description	:link old file to new file
==================================================================================
*/
static int
me2fsLink( struct dentry *old_dentry, struct inode *dir, struct dentry *dentry )
{
	struct inode	*old_inode;
	int				err;

	old_inode = old_dentry->d_inode;

	old_inode->i_ctime = CURRENT_TIME_SEC;
	inode_inc_link_count( old_inode );
	ihold( old_inode );

	if( !( err = me2fsAddLink( dentry, old_inode ) ) )
	{
		d_instantiate( dentry, old_inode );
		return( 0 );
	}

	inode_dec_link_count( old_inode );
	iput( old_inode );

	return( err );

}
/*
==================================================================================
	Function	:me2fsSymlink
	Input		:struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < new linked file >
				 const char *symname
				 < symbolic link path >
	Output		:void
	Return		:void

	Description	:make a symbolic link
==================================================================================
*/
static int
me2fsSymlink( struct inode *dir,
			  struct dentry *dentry,
			  const char *symname )
{
	struct super_block	*sb;
	int					err;
	unsigned			len;
	struct inode		*inode;

	sb	= dir->i_sb;
	len	= strlen( symname ) + 1;

	if( sb->s_blocksize < len )
	{
		return( -ENAMETOOLONG );
	}

	inode = me2fsAllocNewInode( dir, S_IFLNK | S_IRWXUGO, &dentry->d_name );

	if( IS_ERR( inode ) )
	{
		return( PTR_ERR( inode ) );
	}

	/* ------------------------------------------------------------------------ */
	/* slow symlink																*/
	/* ------------------------------------------------------------------------ */
	if( sizeof( ME2FS_I( inode )->i_data ) < len )
	{
		inode->i_op				= &me2fs_symlink_inode_operations;
		inode->i_mapping->a_ops	= &me2fs_aops;
		if( ( err = page_symlink( inode, symname, len ) ) )
		{
			inode_dec_link_count( inode );
			unlock_new_inode( inode );
			iput( inode );
			return( err );
		}
	}
	/* ------------------------------------------------------------------------ */
	/* fast symlink																*/
	/* ------------------------------------------------------------------------ */
	else
	{
		inode->i_op				= &me2fs_fast_symlink_inode_operations;
		memcpy( ( char* )( ME2FS_I( inode )->i_data ), symname, len );
		inode->i_size = len - 1;
	}

	mark_inode_dirty( inode );

	return( addNonDir( dentry, inode ) );
}

/*
==================================================================================
	Function	:me2fsMknod
	Input		:struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < node name to make >
				 umode_t mode
				 < file mode >
				 dev_t rdev
				 < device number >
	Output		:void
	Return		:int
				 < result >

	Description	:make a special file
==================================================================================
*/
static int
me2fsMknod( struct inode *dir,
			struct dentry *dentry,
			umode_t mode,
			dev_t rdev )
{
	struct inode	*inode;
	int				err;

	if( !new_valid_dev( rdev ) )
	{
		return( -EINVAL );
	}

	inode	= me2fsAllocNewInode( dir, mode, &dentry->d_name );
	err		= PTR_ERR( inode );
	if( !IS_ERR( inode ) )
	{
		init_special_inode( inode, inode->i_mode, rdev );
		inode->i_op	= &me2fs_special_inode_operations;
		mark_inode_dirty( inode );
		err			= addNonDir( dentry, inode );
	}

	return( err );
}

/*
==================================================================================
	Function	:me2fsTmpFile
	Input		:struct inode *dir
				 < vfs inode of parent >
				 struct dentry *dentry
				 < tmp file to make >
				 umode_t mode
				 < file mode >
	Output		:void
	Return		:int
				 < result >

	Description	:make a temporary file
==================================================================================
*/
static int
me2fsTmpFile( struct inode *dir,
			  struct dentry *dentry,
			  umode_t mode )
{
	struct inode	*inode;

	inode = me2fsAllocNewInode( dir, mode, NULL );

	if( IS_ERR( inode ) )
	{
		return( PTR_ERR( inode ) );
	}

	inode->i_op				= &me2fs_file_inode_operations;
	inode->i_mapping->a_ops	= &me2fs_aops;
	inode->i_fop			= &me2fs_file_operations;

	mark_inode_dirty( inode );
	d_tmpfile( dentry, inode );
	unlock_new_inode( inode );

	return( 0 );
}
/*
----------------------------------------------------------------------------------

	Helper Functions

----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:addNonDir
	Input		:struct dentry *dentry
				 < dentry to add >
				 struct inode *inode
				 < vfs inode >
	Output		:void
	Return		:int
				 < result >

	Description	:add link and splice dentry and inode
==================================================================================
*/
static inline int
addNonDir( struct dentry *dentry, struct inode *inode )
{
	int		err;

	if( !( err = me2fsAddLink( dentry, inode ) ) )
	{
		DBGPRINT( "<ME2FS>%s:create [%s]\n", __func__, dentry->d_name.name );
		unlock_new_inode( inode );
		d_instantiate( dentry, inode );
		return( 0 );
	}

	DBGPRINT( "<ME2FS>%s:erro add non-dir entry\n", __func__ );

	inode_dec_link_count( inode );
	unlock_new_inode( inode );
	iput( inode );
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
