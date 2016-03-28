/********************************************************************************
	File			: me2fs_file.c
	Description		: File Operations for my ext2 file system

*********************************************************************************/

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_ioctl.h"
#include "me2fs_inode.h"
#include "me2fs_block.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static int me2fsReleaseFile( struct inode *inode, struct file *file );

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
const struct file_operations me2fs_file_operations =
{
	.llseek			= generic_file_llseek,
	//.read			= do_sync_read,
	.read			= new_sync_read,
	//.write			= do_sync_write,
	.write			= new_sync_write,
	.read_iter		= generic_file_read_iter,
	.write_iter		= generic_file_write_iter,
	//.aio_read		= generic_file_aio_read,
	//.aio_write		= generic_file_aio_write,
	.unlocked_ioctl	= me2fsIoctl,
	.compat_ioctl	= me2fsCompatIoctl,
	.mmap			= generic_file_mmap,
	.open			= generic_file_open,
	.release		= me2fsReleaseFile,
	.fsync			= generic_file_fsync,
	.splice_read	= generic_file_splice_read,
	//.splice_write	= generic_file_splice_write,
	.splice_write	= iter_file_splice_write,

};
const struct inode_operations me2fs_file_inode_operations =
{
	.setattr		= me2fsSetAttr,
	.fiemap			= me2fsFiemap,
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
==================================================================================
	Function	:me2fsReleaseFile
	Input		:struct inode *inode
				 < vfs inode >
				 struct file *filp
				 < vfs file object >
	Output		:void
	Return		:void

	Description	:release file.
==================================================================================
*/
static int me2fsReleaseFile( struct inode *inode, struct file *filp )
{
	if( filp->f_mode & FMODE_WRITE )
	{
		mutex_lock( &ME2FS_I( inode )->truncate_mutex );
		{
			me2fsDiscardReservation( inode );
		}
		mutex_unlock( &ME2FS_I( inode )->truncate_mutex );
	}

	return( 0 );
}
