/********************************************************************************
	File			: me2fs_ioctl.c
	Description		: Operations of me2fs ioctl

*********************************************************************************/
#include <linux/mount.h>
#include <linux/compat.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_inode.h"
#include "me2fs_block.h"


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
long me2fsIoctl( struct file *filp, unsigned int cmd, unsigned long arg )
{
	struct inode			*inode;
	struct me2fs_inode_info	*mei;
	unsigned int			flags;
	int						ret;
	unsigned int			oldflags;
	__u32					generation;
	unsigned short			rsv_window_size;

	DBGPRINT( "<ME2FS>ioctl cmd = %u, arg = %lu\n", cmd, arg );

	inode	= file_inode( filp );
	mei		= ME2FS_I( inode );

	switch( cmd )
	{
	case	EXT2_IOC_GETFLAGS:
		/* get flags: set me2fs inode flags to vfs inode flags					*/
		me2fsSetMe2fsInodeFlags( mei );
		flags = mei->i_flags & EXT2_FL_USER_VISIBLE;
		return( put_user( flags, ( int __user* )arg ) );
	case	EXT2_IOC_SETFLAGS:
		if( ( ret = mnt_want_write_file( filp ) ) )
		{
			return( ret );
		}

		if( !inode_owner_or_capable( inode ) )
		{
			ret = -EACCES;
			mnt_drop_write_file( filp );
			return( ret );
		}

		if( get_user( flags, ( int __user* )arg ) )
		{
			ret = -EFAULT;
			mnt_drop_write_file( filp );
			return( ret );
		}

		if( S_ISDIR( inode->i_mode ) )
		{
			/* use user flags, so do nothing									*/
		}
		else if( S_ISREG( inode->i_mode ) )
		{
			flags = flags & EXT2_REG_FLMASK;
		}
		else
		{
			flags = flags & EXT2_OTHER_FLMASK;
		}

		mutex_lock( &inode->i_mutex );
		{
			if( IS_NOQUOTA( inode ) )
			{
				mutex_unlock( &inode->i_mutex );
				ret = -EPERM;
				mnt_drop_write_file( filp );
				return( ret );
			}

			oldflags = mei->i_flags;
			
			/* ---------------------------------------------------------------- */
			/* IMMUTABLE and APPEND_ONLY flags can only be changed by the		*/
			/* relevant capability.												*/
			/* ---------------------------------------------------------------- */
			if( ( flags ^ oldflags ) & ( EXT2_APPEND_FL | EXT2_IMMUTABLE_FL ) )
			{
				if( !capable( CAP_LINUX_IMMUTABLE ) )
				{
					mutex_unlock( &inode->i_mutex );
					ret = -EPERM;
					mnt_drop_write_file( filp );
					return( ret );
				}
			}

			flags			&= EXT2_FL_USER_MODIFIABLE;
			flags			|= oldflags & ~EXT2_FL_USER_MODIFIABLE;
			mei->i_flags	=  flags;

			me2fsSetVfsInodeFlags( inode );
			inode->i_ctime	= CURRENT_TIME_SEC;
		}
		mutex_unlock( &inode->i_mutex );

		mark_inode_dirty( inode );
		mnt_drop_write_file( filp );
		return( ret );
	case	EXT2_IOC_GETVERSION:
		return( put_user( inode->i_generation, ( int __user* )arg ) );
	case	EXT2_IOC_SETVERSION:

		if( !inode_owner_or_capable( inode ) )
		{
			return( -EPERM );
		}

		if( ( ret = mnt_want_write_file( filp ) ) )
		{
			return( ret );
		}

		if( get_user( generation, ( int __user* )arg ) )
		{
			ret = -EFAULT;
			mnt_drop_write_file( filp );
			return( ret );
		}

		mutex_lock( &inode->i_mutex );
		{
			inode->i_ctime		= CURRENT_TIME_SEC;
			inode->i_generation	= generation;
		}
		mutex_unlock( &inode->i_mutex );

		mark_inode_dirty( inode );
		mnt_drop_write_file( filp );

		return( ret );
	case	EXT2_IOC_GETRSVSZ:
		if( ( ME2FS_SB( inode->i_sb )->s_mount_opt & EXT2_MOUNT_RESERVATION ) &&
			( S_ISREG( inode->i_mode ) ) &&
			( mei->i_block_alloc_info ) )
		{
			rsv_window_size =
						mei->i_block_alloc_info->rsv_window_node.rsv_goal_size;
			return( put_user( rsv_window_size, ( int __user* )arg ) );
		}
		return( -ENOTTY );
	case	EXT2_IOC_SETRSVSZ:
		if( !( ME2FS_SB( inode->i_sb )->s_mount_opt & EXT2_MOUNT_RESERVATION ) ||
			!S_ISREG( inode->i_mode ) )
		{
			return( -ENOTTY );
		}
		if( get_user( rsv_window_size, ( int __user* )arg ) )
		{
			return( -EFAULT );
		}
		if( ( ret = mnt_want_write_file( filp ) ) )
		{
			return( ret );
		}
		if( EXT2_MAX_RESERVE_BLOCKS < rsv_window_size )
		{
			rsv_window_size = EXT2_MAX_RESERVE_BLOCKS;
		}
		/* -------------------------------------------------------------------- */
		/* need to allocate reservation structure for this inode before set		*/
		/* the window size														*/
		/* -------------------------------------------------------------------- */
		mutex_lock( &mei->truncate_mutex );
		{
			if( !mei->i_block_alloc_info )
			{
				me2fsInitBlockAllocInfo( inode );
			}
			if( mei->i_block_alloc_info )
			{
				struct ext2_reserve_window_node	*rsv;

				rsv = &mei->i_block_alloc_info->rsv_window_node;
				rsv->rsv_goal_size = rsv_window_size;
			}
		}
		mutex_unlock( &mei->truncate_mutex );
		mnt_drop_write_file( filp );
		return( 0 );
	default:
		break;
	}

	return( -ENOTTY );
}

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
long me2fsCompatIoctl( struct file *file, unsigned int cmd, unsigned long arg )
{
	switch( cmd )
	{
	case	EXT2_IOC32_GETFLAGS:
		cmd = EXT2_IOC_GETFLAGS;
		break;
	case	EXT2_IOC32_SETFLAGS:
		cmd = EXT2_IOC_SETFLAGS;
		break;
	case	EXT2_IOC32_GETVERSION:
		cmd = EXT2_IOC_GETVERSION;
		break;
	case	EXT2_IOC32_SETVERSION:
		cmd = EXT2_IOC_SETVERSION;
		break;
	default:
		return( -ENOIOCTLCMD );
	}

	return( me2fsIoctl( file, cmd, ( unsigned long )compat_ptr( arg ) ) );
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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/
