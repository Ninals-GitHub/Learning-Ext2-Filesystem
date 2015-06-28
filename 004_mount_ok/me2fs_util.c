/********************************************************************************
	File			: me2fs_util.c
	Description		: utilities of my ext2 simple filesystem

********************************************************************************/
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include "me2fs.h"
#include "me2fs_util.h"

/*
=================================================================================

	Prototype Statement

=================================================================================
*/


/*
=================================================================================

	DEFINES

=================================================================================
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
	Function	:debugWriteProc
	Input		:struct file *file
				 < file struct >
				 const char *buf
				 < user data >
				 unsigned long count
				 < user data size >
				 void *data
				 < private data >
	Output		:void
	Return		:void

	Description	:write function on proc file system
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int debugWriteProc( struct file *file,
					const char *buf,
					unsigned long count,
					void *data )
{
	char str[ 20 ];
	int ret = -1;

	MOD_INC_USE_COUNT;

	if( count > sizeof( str ) - 1 )
	{
		goto done;
	}

	if( copy_from_user( str, buf, count ) )
	{
		goto done;
	}

	ret = count;

	str[ count ] = '\n';

	DBGPRINT( str );

done:
	MOD_DEC_USE_COUNT;
	return( ret );
}

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
