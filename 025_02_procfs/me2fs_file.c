/********************************************************************************
	File			: me2fs_file.c
	Description		: File Operations for my ext2 file system

*********************************************************************************/

#include "me2fs.h"
#include "me2fs_util.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
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
	//.unlocked_ioctl	= me2fsIoctl,
	.mmap			= generic_file_mmap,
	.open			= generic_file_open,
	//.release		= me2fsReleaseFile,
	.fsync			= generic_file_fsync,
	.splice_read	= generic_file_splice_read,
	//.splice_write	= generic_file_splice_write,
	.splice_write	= iter_file_splice_write,

};
const struct inode_operations me2fs_file_inode_operations;

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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
