/*********************************************************************************
	File			: me2fs_ioctl.h
	Description		: Definitions for me2fs ioctl

*********************************************************************************/
#ifndef	__FORMAT_H__
#define	__FORMAT_H__


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

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsIoctl
	Input		:struct file *filp
				 < vfs file object >
				 unsigned int cmd
				 < command for ioctl >
				 unsigned long arg
				 < user buffer >
	Output		:void
	Return		:long
				 < result >

	Description	:i/o contorl of file
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
long me2fsIoctl( struct file *filp, unsigned int cmd, unsigned long arg );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsCompatIoctl
	Input		:struct file *filp
				 < vfs file object >
				 unsigned int cmd
				 < command for ioctl >
				 unsigned long arg
				 < user buffer >
	Output		:void
	Return		:long
				 < result >

	Description	:32 bit compatible ioctl
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
long me2fsCompatIoctl( struct file *file, unsigned int cmd, unsigned long arg );

#endif	// __FORMAT_H__
