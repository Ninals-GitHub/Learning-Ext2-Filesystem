/********************************************************************************
	File			: me2fs_super.c
	Description		: my ext2 file sytem main routine

********************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>

#include "me2fs.h"
#include "me2fs_super.h"
#include "me2fs_util.h"
#include "me2fs_sysfs.h"
#include "me2fs_xattr.h"


/*
=================================================================================

	Prototype Statement

=================================================================================
*/
static struct dentry *me2fs_mount( struct file_system_type *fs_type,
								  int flags,
								  const char *dev_name,
								  void *data );

/*
=================================================================================

	DEFINES

=================================================================================
*/

#define	ME2FS_VERSION_STRING		"me2fs_0.01"

#define	ME2FS_PROC_MODULE_NAME		"me2fs"

#define	ME2FS_MODULE_DESC			"My ex2 file system (me2fs)"
#define	ME2FS_MODULE_AUTHOR			"yabusame2001 <mail@softwaretechnique.jp>"

/*
==================================================================================

	Management

==================================================================================
*/
/*
---------------------------------------------------------------------------------
	File system type
---------------------------------------------------------------------------------
*/
static struct file_system_type me2fs_fstype =
{
	.name		= "me2fs",
	.mount		= me2fs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

/*
---------------------------------------------------------------------------------
	procfs
---------------------------------------------------------------------------------
*/
static struct proc_dir_entry *me2fs_proc_root;

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetProcRoot
	Input		:void
	Output		:void
	Return		:struct proc_dir_entry
				 < me2fs proc root >

	Description	:get proc root of me2fs
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct proc_dir_entry* me2fsGetProcRoot( void )
{
	return( me2fs_proc_root );
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
	Function	:me2fs_mount
	Input		:struct file_system_type *fs_type
				 < file system type >
				 int flags
				 < flags for mounting >
				 const char *dev_name
				 < device file name >
				 void *data
				 < options >
	Output		:void
	Return		:dentry *
				 < root dentry object >

	Description	:mount a ext2 fs
==================================================================================
*/
static struct dentry *me2fs_mount( struct file_system_type *fs_type,
								  int flags,
								  const char *dev_name,
								  void *data )
{
	return( me2fsMountBlockDev( fs_type, flags, dev_name, data ) );
}

/*
==================================================================================
	Function	:initMe2fs
	Input		:void
	Output		:void
	Return		:void

	Description	:initialize Me2fs module
==================================================================================
*/
static int __init initMe2fs( void )
{
	int		error;

	DBGPRINT( "(Me2FS)Hello, world\n" );

	error = me2fsInitKset( );

	if( error )
	{
		return( error );
	}

	me2fs_proc_root = proc_mkdir( "fs/me2fs", NULL );

	error = me2fsInitInodeCache( );

	if( error )
	{
		me2fsUnregisterKset( );
		
		if( me2fs_proc_root )
		{
			remove_proc_entry( "fs/me2fs", NULL );
		}
		return( error );
	}

	error = me2fsInitXattr( );

	if( error )
	{
		me2fsUnregisterKset( );
		
		if( me2fs_proc_root )
		{
			remove_proc_entry( "fs/me2fs", NULL );
		}
		me2fsDestroyInodeCache( );
		return( error );
	}

	error = register_filesystem( &me2fs_fstype );

	if( error )
	{
		me2fsUnregisterKset( );

		if( me2fs_proc_root )
		{
			remove_proc_entry( "fs/me2fs", NULL );
		}
		me2fsDestroyInodeCache( );
		me2fsExitXattr( );
	}

	return( error );
}

/*
==================================================================================
	Function	:exitMe2fs
	Input		:void
	Output		:void
	Return		:void

	Description	:clean up vsfs module
==================================================================================
*/
static void __exit exitMe2fs( void )
{
	DBGPRINT( "(ME2FS)Good-bye!\n" );

	me2fsUnregisterKset( );

	me2fsDestroyInodeCache( );

	unregister_filesystem( &me2fs_fstype );

	if( me2fs_proc_root )
	{
		remove_proc_entry( "fs/me2fs", NULL );
	}

	me2fsExitXattr( );
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

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Kernel Registrations >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
---------------------------------------------------------------------------------
	General Module Information
---------------------------------------------------------------------------------
*/
module_init( initMe2fs );
module_exit( exitMe2fs );

MODULE_AUTHOR( ME2FS_MODULE_AUTHOR );
MODULE_DESCRIPTION( ME2FS_MODULE_DESC );
MODULE_LICENSE( "GPL" );
