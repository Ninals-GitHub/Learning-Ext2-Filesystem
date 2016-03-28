/********************************************************************************
	File			: me2fs_super.c
	Description		: super block operations for my ext2 file sytem

********************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/parser.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/quotaops.h>

#include "me2fs.h"
#include "me2fs_util.h"
#include "me2fs_super.h"
#include "me2fs_block.h"
#include "me2fs_inode.h"
#include "me2fs_ialloc.h"
#include "me2fs_sysfs.h"
#include "me2fs_xattr.h"


/*
=================================================================================

	Prototype Statement

=================================================================================
*/
/*
---------------------------------------------------------------------------------
	For Super Block Operations
---------------------------------------------------------------------------------
*/
static int me2fsFillSuperBlock( struct super_block *sb,
								void *data,
								int silent );
static struct inode *me2fsAllocInode( struct super_block *sb );
static void me2fsDestroyInode( struct inode *inode );
static void me2fsPutSuperBlock( struct super_block *sb );
static int me2fsSyncFs( struct super_block *sb, int wait );
static int me2fsStatfs( struct dentry *dentry, struct kstatfs *buf );
static int me2fsRemount( struct super_block *sb,
						 int *flags,
						 char *data );
static int me2fsShowOptions( struct seq_file *seq, struct dentry *root );
static int me2fsFreeze( struct super_block *sb );
static int me2fsUnfreeze( struct super_block *sb );
static ssize_t
me2fsQuotaRead( struct super_block *sb,
				int type,
				char *data,
				size_t len,
				loff_t off );
static ssize_t
me2fsQuotaWrite( struct super_block *sb,
				 int type,
				 const char *data,
				 size_t len,
				 loff_t off );

/*
---------------------------------------------------------------------------------
	For Seq File Operations
---------------------------------------------------------------------------------
*/
static int optionsOpenFs( struct inode *inode, struct file *file );
static int optionsSeqShow( struct seq_file *seq, void *offset );

/*
---------------------------------------------------------------------------------
	Helper Functions
---------------------------------------------------------------------------------
*/
static void dbgPrintExt2SB( struct ext2_super_block *esb );
static void
dbgPrintExt2Bgd( struct ext2_group_desc *group_desc, unsigned long group_no );
static void dbgPrintMe2fsInfo( struct me2fs_sb_info *msi );
static loff_t me2fsMaxFileSize( struct super_block *sb );
static void me2fsInitInodeOnce( void *object );
static unsigned long
getDescriptorLocation( struct super_block *sb,
					   unsigned long logic_sb_block,
					   int num_bg );
static void me2fsSyncSuper( struct super_block *sb,
							struct ext2_super_block *esb,
							int wait );
static void clearSuperError( struct super_block *sb );
static int parseOptions( char *options, struct super_block *sb );
static unsigned long getSbBlock( void **data );
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
---------------------------------------------------------------------------------
	Super Block Operations
---------------------------------------------------------------------------------
*/
//static int me2fs_write_inode( struct inode* inode, struct writeback_control *wbc )
//{ DBGPRINT( "<ME2FS>write_inode\n" ); return 0; }
//static int me2fs_drop_inode( struct inode *inode )
//{ DBGPRINT( "<ME2FS>drop_inode\n" ); return 0; }
//static void me2fs_evict_inode( struct inode *inode )
//{ DBGPRINT( "<ME2FS>evict_inode\n" ); }
//static int me2fs_sync_fs( struct super_block *sb, int wait )
//{ DBGPRINT( "<ME2FS>sync_fs\n" ); return 0; }
//static int me2fs_freeze_fs( struct super_block *sb )
//{ DBGPRINT( "<ME2FS>freeze_fs\n" ); return 0; }
//static int me2fs_unfreeze_fs( struct super_block *sb )
//{ DBGPRINT( "<ME2FS>unfreeze_fs\n" ); return 0; }
//static int me2fs_statfs( struct dentry *dentry, struct kstatfs *buf )
//{ DBGPRINT( "<ME2FS>statfs\n" ); return 0; }
//static int me2fs_remount_fs( struct super_block *sb, int *len, char *buf )
//{ DBGPRINT( "<ME2FS>remount_fs\n" ); return 0; }
//static void me2fs_umount_begin( struct super_block *sb )
//{ DBGPRINT( "<ME2FS>unmount_begin\n" ); }
//static int me2fs_show_options( struct seq_file *seq_file, struct dentry *dentry )
//{ DBGPRINT( "<ME2FS>show_options\n" ); return 0; }


static struct super_operations me2fs_super_ops = {
	.alloc_inode	= me2fsAllocInode,
	.destroy_inode	= me2fsDestroyInode,
//	.dirty_inode	= me2fs_dirty_inode,
	.write_inode	= me2fsWriteInode,
//	.drop_inode = me2fs_drop_inode,
	.evict_inode	= me2fsEvictInode,
	.put_super		= me2fsPutSuperBlock,
	.sync_fs		= me2fsSyncFs,
	.freeze_fs		= me2fsFreeze,
	.unfreeze_fs	= me2fsUnfreeze,
	.statfs			= me2fsStatfs,
	.remount_fs		= me2fsRemount,
//	.umount_begin	= me2fs_umount_begin,
	.show_options	= me2fsShowOptions,

	.quota_read		= me2fsQuotaRead,
	.quota_write	= me2fsQuotaWrite,
};

/*
---------------------------------------------------------------------------------
	Export Operations
---------------------------------------------------------------------------------
*/
//static struct export_operations me2fs_export_ops;

/*
---------------------------------------------------------------------------------
	Extended Attribute Handlers
---------------------------------------------------------------------------------
*/
#if 0
const static struct xattr_handler *me2fs_xattr_handler[ ] =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

#endif

/*
---------------------------------------------------------------------------------
	seq options file operations
---------------------------------------------------------------------------------
*/
static const struct file_operations me2fs_seq_options_fops =
{
	.owner		= THIS_MODULE,
	.open		= optionsOpenFs,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

/*
---------------------------------------------------------------------------------
	inode cache
---------------------------------------------------------------------------------
*/
static struct kmem_cache *me2fs_inode_cachep;

/*
---------------------------------------------------------------------------------
	Token of Options
---------------------------------------------------------------------------------
*/
enum
{
	Opt_bsd_df,
	Opt_minix_df,
	Opt_grpid,
	Opt_nogrpid,
	Opt_resgid,
	Opt_resuid,
	Opt_sb,
	Opt_err_cont,
	Opt_err_panic,
	Opt_err_ro,
	Opt_nouid32,
	Opt_nocheck,
	Opt_debug,
	Opt_oldalloc,
	Opt_orlov,
	Opt_nobh,
	Opt_user_xattr,
	Opt_nouser_xattr,
	Opt_acl,
	Opt_noacl,
	Opt_xip,
	Opt_ignore,
	Opt_err,
	Opt_quota,
	Opt_usrquota,
	Opt_grpquota,
	Opt_reservation,
	Opt_noreservation,
};

static const match_table_t tokens =
{
	{ Opt_bsd_df,			"bsddf"				},
	{ Opt_minix_df,			"minixdf"			},
	{ Opt_grpid,			"grpid"				},
	{ Opt_grpid,			"bsdgroups"			},
	{ Opt_nogrpid,			"nogrpid"			},
	{ Opt_nogrpid,			"sysvgroups"		},
	{ Opt_resgid,			"resgid=%u"			},
	{ Opt_resuid,			"resuid=%u"			},
	{ Opt_sb,				"sb=%u"				},
	{ Opt_err_cont,			"errors=continue"	},
	{ Opt_err_panic,		"errors=panic"		},
	{ Opt_err_ro,			"errors=remount-ro"	},
	{ Opt_nouid32,			"nouid32"			},
	{ Opt_nocheck,			"check=none"		},
	{ Opt_nocheck,			"nocheck"			},
	{ Opt_debug,			"debug"				},
	{ Opt_oldalloc,			"oldalloc"			},
	{ Opt_orlov,			"orlov"				},
	{ Opt_nobh,				"nobh"				},
	{ Opt_user_xattr,		"user_xattr"		},
	{ Opt_nouser_xattr,		"nouser_xattr"		},
	{ Opt_acl,				"acl"				},
	{ Opt_noacl,			"noacl"				},
	{ Opt_xip,				"xip"				},
	{ Opt_grpquota,			"grpquota"			},
	{ Opt_quota,			"quota"				},
	{ Opt_ignore,			"noquota"			},
	{ Opt_usrquota,			"usrquota"			},
	{ Opt_reservation,		"reservation"		},
	{ Opt_noreservation,	"noreservation"		},
	{ Opt_err,				NULL				},
};

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitInodeCache
	Input		:void
	Output		:void
	Return		:void

	Description	:initialize inode cache
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitInodeCache( void )
{
	me2fs_inode_cachep = kmem_cache_create( "me2fs_inode_cachep",
											sizeof( struct me2fs_inode_info ),
											0,
											( SLAB_RECLAIM_ACCOUNT |
											  SLAB_MEM_SPREAD ),
											me2fsInitInodeOnce );
	
	if( !me2fs_inode_cachep )
	{
		return( -ENOMEM );
	}

	return( 0 );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsDestroyInodeCache
	Input		:void
	Output		:void
	Return		:void

	Description	:destroy inode cache
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsDestroyInodeCache( void )
{
	kmem_cache_destroy( me2fs_inode_cachep );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsMountBlockDev
	Input		:struct file_system_type *fs_type
				 < file system type >
				 int flags
				 < mount flags >
				 const char *dev_name
				 < device name >
				 void *data
				 < user data >
	Output		:void
	Return		:struct dentry *
				 < root dentry >

	Description	:mount me2fs over block device
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct dentry*
me2fsMountBlockDev( struct file_system_type *fs_type,
					int flags,
					const char *dev_name,
					void *data )
{
	return( mount_bdev( fs_type, flags, dev_name, data, me2fsFillSuperBlock ) );
}

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsWriteSuper
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:write super block
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsWriteSuper( struct super_block *sb )
{
	if( !( sb->s_flags & MS_RDONLY ) )
	{
		me2fsSyncFs( sb, 1 );
	}
}

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Local Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
==================================================================================
	Function	:me2fsFillSuperBlock
	Input		:struct super_block *sb
				 < vfs super block object >
				 void *data
				 < user data >
				 int silent
				 < silent flag >
	Output		:void
	Return		:void

	Description	:fill my ext2 super block object
==================================================================================
*/
static int me2fsFillSuperBlock( struct super_block *sb,
								void *data,
								int silent )
{
	struct buffer_head		*bh;
	struct ext2_super_block	*esb;
	struct me2fs_sb_info	*msi;
	struct inode			*root;
	int						block_size;
	int						ret = -EINVAL;
	int						i;
	int						err;
	unsigned long			sb_block;
	unsigned long			def_mount_opts;

	/* ------------------------------------------------------------------------ */
	/* parse mount options to get super block location							*/
	/* ------------------------------------------------------------------------ */
	sb_block = getSbBlock( &data );

	/* ------------------------------------------------------------------------ */
	/* allocate memory to me2fs_sb_info											*/
	/* ------------------------------------------------------------------------ */
	msi = kzalloc( sizeof( struct me2fs_sb_info ), GFP_KERNEL );

	if( !msi )
	{
		ME2FS_ERROR( "<ME2FS>error: unable to alloc me2fs_sb_info\n" );
		ret = -ENOMEM;
		return( ret );
	}

	/* set me2fs information to vfs super block									*/
	sb->s_fs_info	= ( void* )msi;

	/* ------------------------------------------------------------------------ */
	/* allocate memory to spin locks for block group							*/
	/* ------------------------------------------------------------------------ */
	msi->s_blockgroup_lock = kzalloc( sizeof( struct blockgroup_lock ),
									  GFP_KERNEL );
	if( !msi->s_blockgroup_lock )
	{
		ME2FS_ERROR( "<ME2FS>error: unabel to alloc s_blockgroup_lock\n" );
		kfree( msi );
		return( -ENOMEM );
	}

	/* ------------------------------------------------------------------------ */
	/* set device's block size and size bits to super block						*/
	/* ------------------------------------------------------------------------ */
	block_size = sb_min_blocksize( sb, BLOCK_SIZE );

	DBGPRINT( "<ME2FS>Fill Super! block_size = %d\n", block_size );
	DBGPRINT( "<ME2FS>s_blocksize_bits = %d\n", sb->s_blocksize_bits );
	DBGPRINT( "<ME2FS>default block size is : %d\n", BLOCK_SIZE );

	if( !block_size )
	{
		ME2FS_ERROR( "<ME2FS>error: unable to set blocksize\n" );
		goto error_read_sb;
	}

	/* ------------------------------------------------------------------------ */
	/* read super block															*/
	/* ------------------------------------------------------------------------ */
	if( !( bh = sb_bread( sb, sb_block ) ) )
	{
		ME2FS_ERROR( "<ME2FS>failed to bread super block\n" );
		goto error_read_sb;
	}

	esb = ( struct ext2_super_block* )( bh->b_data );

	/* ------------------------------------------------------------------------ */
	/* check magic number														*/
	/* ------------------------------------------------------------------------ */
	sb->s_magic = le16_to_cpu( esb->s_magic );

	if( sb->s_magic != ME2FS_SUPER_MAGIC )
	{
		ME2FS_ERROR( "<ME2FS>error : magic of super block is %lu\n", sb->s_magic );
		goto error_mount;
	}

	/* ------------------------------------------------------------------------ */
	/* check default options and mount options									*/
	/* ------------------------------------------------------------------------ */
	def_mount_opts = le32_to_cpu( esb->s_default_mount_opts );

	if( def_mount_opts & EXT2_DEFM_DEBUG )
	{
		msi->s_mount_opt |= EXT2_MOUNT_DEBUG;
		DBGPRINT( "<ME2FS>%s:option:set debug flag\n", __func__ );
	}
	if( def_mount_opts & EXT2_DEFM_BSDGROUPS )
	{
		msi->s_mount_opt |= EXT2_MOUNT_GRPID;
		DBGPRINT( "<ME2FS>%s:option:set grpid flag\n", __func__ );
	}
	if( def_mount_opts & EXT2_DEFM_UID16 )
	{
		msi->s_mount_opt |= EXT2_MOUNT_NO_UID32;
		DBGPRINT( "<ME2FS>%s:option:set nou_id32 flag\n", __func__ );
	}
	if( def_mount_opts & EXT2_DEFM_XATTR_USER )
	{
		msi->s_mount_opt |= EXT2_MOUNT_XATTR_USER;
		DBGPRINT( "<ME2FS>%s:option:set xattr_user flag\n", __func__ );
	}
	if( def_mount_opts & EXT2_DEFM_ACL )
	{
		msi->s_mount_opt |= EXT2_MOUNT_POSIX_ACL;
		DBGPRINT( "<ME2FS>%s:option:set posix_acl flag\n", __func__ );
	}
	if( le16_to_cpu( esb->s_errors ) == EXT2_ERRORS_PANIC )
	{
		msi->s_mount_opt |= EXT2_MOUNT_ERRORS_PANIC;
	}
	else if( le16_to_cpu( esb->s_errors ) == EXT2_ERRORS_CONTINUE )
	{
		msi->s_mount_opt |= EXT2_MOUNT_ERRORS_CONT;
	}
	else
	{
		msi->s_mount_opt |= EXT2_MOUNT_ERRORS_RO;
	}

	/* deaults(disk information cache)											*/
	//msi->s_resuid = make_kuid( &init_user_ns, le16_to_cpu( esb->s_def_resuid ) );
	//msi->s_resgid = make_kgid( &init_user_ns, le16_to_cpu( esb->s_def_resgid ) );

	if( !parseOptions( ( char* )data, sb ) )
	{
		goto error_mount;
	}

	msi->s_mount_opt |= EXT2_MOUNT_RESERVATION;
	msi->s_mount_opt &= ~EXT2_MOUNT_OLDALLOC;
	msi->s_mount_opt &= ~EXT2_MOUNT_MINIX_DF;

	sb->s_flags = sb->s_flags & ~MS_POSIXACL;
	if( msi->s_mount_opt & EXT2_MOUNT_POSIX_ACL )
	{
		sb->s_flags |= MS_POSIXACL;
	}

	if( ( le32_to_cpu( esb->s_rev_level ) == EXT2_GOOD_OLD_REV ) &&
		esb->s_feature_incompat & cpu_to_le32( ~EXT2_FEATURE_INCOMPAT_SUPP ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:error:unsupported optional features(%x)",
					 __func__,
					 le32_to_cpu( esb->s_feature_incompat &
					 			  cpu_to_le32( ~EXT2_FEATURE_INCOMPAT_SUPP ) ) );
		goto error_mount;
	}
	if( !( sb->s_flags & MS_RDONLY ) &&
		esb->s_feature_incompat & cpu_to_le32( ~EXT2_FEATURE_INCOMPAT_SUPP ) )
	{
		ME2FS_ERROR( "<ME2FS>%s:error:unsupported optional features(%x)",
					 __func__,
					 le32_to_cpu( esb->s_feature_incompat &
					 			  cpu_to_le32( ~EXT2_FEATURE_INCOMPAT_SUPP ) ) );
		goto error_mount;
	}

	/* ------------------------------------------------------------------------ */
	/* check revison															*/
	/* ------------------------------------------------------------------------ */
	if( ME2FS_OLD_REV == le32_to_cpu( esb->s_rev_level ) )
	{
		ME2FS_ERROR( "<ME2FS>error : cannot mount old revision\n" );
		goto error_mount;
	}

	dbgPrintExt2SB( esb );

	/* ------------------------------------------------------------------------ */
	/* set up me2fs super block information										*/
	/* ------------------------------------------------------------------------ */
	/* buffer cache information													*/
	msi->s_esb				= esb;
	msi->s_sbh				= bh;
	/* super block(disk information cache)										*/
	msi->s_sb_block			= sb_block;
	//msi->s_mount_opt		= le32_to_cpu( esb->s_default_mount_opts );
	msi->s_mount_state		= le16_to_cpu( esb->s_state );

	if( msi->s_mount_state != EXT2_VALID_FS )
	{
		ME2FS_ERROR( "<ME2FS>error : cannot mount invalid fs\n" );
		goto error_mount;
	}

	if( le16_to_cpu( esb->s_errors ) != EXT2_ERRORS_CONTINUE )
	{
		if( le16_to_cpu( esb->s_errors ) != EXT2_ERRORS_CONTINUE )
		{
			ME2FS_ERROR( "<ME2FS>error : cannot mount read only fs\n" );
		}
		else
		{
			ME2FS_ERROR( "<ME2FS>error : fs panic detected\n" );
		}
		goto error_mount;
		
	}

	if( le32_to_cpu( esb->s_rev_level ) != EXT2_DYNAMIC_REV )
	{
		ME2FS_ERROR( "<ME2FS>error : cannot mount unsupported revision\n" );
		goto error_mount;
	}

	/* inode(disk information cache)											*/
	msi->s_inode_size		= le16_to_cpu( esb->s_inode_size );
	if( ( msi->s_inode_size < 128  ) ||
		!is_power_of_2( msi->s_inode_size ) ||
		( block_size < msi->s_inode_size ) )
	{
		ME2FS_ERROR( "<ME2FS>error : cannot mount unsupported inode size %u\n",
				  msi->s_inode_size );
		goto error_mount;
	}
	msi->s_first_ino		= le32_to_cpu( esb->s_first_ino );
	msi->s_inodes_per_group	= le32_to_cpu( esb->s_inodes_per_group );
	msi->s_inodes_per_block	= sb->s_blocksize / msi->s_inode_size;
	if( msi->s_inodes_per_block == 0 )
	{
		ME2FS_ERROR( "<ME2FS>error : bad inodes per block\n" );
		goto error_mount;
	}
	msi->s_itb_per_group	= msi->s_inodes_per_group / msi->s_inodes_per_block;
	/* group(disk information cache)											*/
	msi->s_blocks_per_group	= le32_to_cpu( esb->s_blocks_per_group );
	if( msi->s_blocks_per_group == 0 )
	{
		ME2FS_ERROR( "<ME2FS>eroor : bad blocks per group\n" );
		goto error_mount;
	}
	msi->s_groups_count		= ( ( le32_to_cpu( esb->s_blocks_count )
								- le32_to_cpu( esb->s_first_data_block ) - 1 )
							  / msi->s_blocks_per_group ) + 1;
	msi->s_desc_per_block	= sb->s_blocksize / sizeof( struct ext2_group_desc );
	msi->s_gdb_count		= ( msi->s_groups_count + msi->s_desc_per_block - 1 )
							  / msi->s_desc_per_block;
	/* fragment(disk information cache)											*/
	msi->s_frag_size		= 1024 << le32_to_cpu( esb->s_log_frag_size );
	if( msi->s_frag_size == 0 )
	{
		ME2FS_ERROR( "<ME2FS>eroor : bad fragment size\n" );
		goto error_mount;
	}
	msi->s_frags_per_block	= sb->s_blocksize / msi->s_frag_size;
	msi->s_frags_per_group	= le32_to_cpu( esb->s_frags_per_group );

	/* deaults(disk information cache)											*/
	msi->s_resuid = make_kuid( &init_user_ns, le16_to_cpu( esb->s_def_resuid ) );
	msi->s_resgid = make_kgid( &init_user_ns, le16_to_cpu( esb->s_def_resgid ) );
	
	dbgPrintMe2fsInfo( msi );

	/* ------------------------------------------------------------------------ */
	/* make procfs entries														*/
	/* ------------------------------------------------------------------------ */
	if( me2fsGetProcRoot( ) )
	{
		msi->s_proc = proc_mkdir( sb->s_id, me2fsGetProcRoot( ) );
	}

	if( msi->s_proc )
	{
		proc_create_data( "options", S_IRUGO, msi->s_proc,
						  &me2fs_seq_options_fops, sb );
	}
	
	/* ------------------------------------------------------------------------ */
	/* read block group descriptor table										*/
	/* ------------------------------------------------------------------------ */
	msi->s_group_desc = kmalloc( msi->s_gdb_count * sizeof( struct buffer_head* ),
								 GFP_KERNEL );
	
	if( !msi->s_group_desc )
	{
		ME2FS_ERROR( "<ME2FS>error : alloc memory for group desc is failed\n" );
		goto error_mount_proc;
	}

	for( i = 0 ; i < msi->s_gdb_count ; i++ )
	{
		unsigned long	block;

		block = getDescriptorLocation( sb, sb_block, i );
		if( !( msi->s_group_desc[ i ] = sb_bread( sb, block ) ) )
		//if( !( msi->s_group_desc[ i ] = sb_bread( sb, sb_block + i + 1 ) ) )
		{
			ME2FS_ERROR( "<ME2FS>error : cannot read " );
			ME2FS_ERROR( "block group descriptor[ group=%d ]\n", i );
			goto error_mount_phase2;
		}
	}

	/* ------------------------------------------------------------------------ */
	/* sanity check for group descriptors										*/
	/* ------------------------------------------------------------------------ */
	for( i = 0 ; i < msi->s_groups_count ; i++ )
	{
		struct ext2_group_desc	*gdesc;
		unsigned long			first_block;
		unsigned long			last_block;
		unsigned long			ar_block;

		DBGPRINT( "<ME2FS>Block Count %d\n", i );

		if( !( gdesc = me2fsGetGroupDescriptor( sb, i ) ) )
		{
			/* corresponding group descriptor does not exist					*/
			goto error_mount_phase2;
		}

		first_block	= ext2GetFirstBlockNum( sb, i );

		if( i == ( msi->s_groups_count - 1 ) )
		{
			last_block	= le32_to_cpu( esb->s_blocks_count ) - 1;
		}
		else
		{
			last_block	= first_block + ( esb->s_blocks_per_group - 1 );
		}

		DBGPRINT( "<ME2FS>first = %lu, last = %lu\n", first_block, last_block );

		ar_block = le32_to_cpu( gdesc->bg_block_bitmap );

		if( ( ar_block < first_block ) || ( last_block < ar_block ) )
		{
			ME2FS_ERROR( "<ME2FS>error : block num of block bitmap is " );
			ME2FS_ERROR( "insanity [ group=%d, first=%lu, block=%lu, last=%lu ]\n",
					  i, first_block, ar_block, last_block );
			goto error_mount_phase2;
		}

		ar_block = le32_to_cpu( gdesc->bg_inode_bitmap );

		if( ( ar_block < first_block ) || ( last_block < ar_block ) )
		{
			ME2FS_ERROR( "<ME2FS>error : block num of inode bitmap is " );
			ME2FS_ERROR( "insanity [ group=%d, first=%lu, block=%lu, last=%lu ]\n",
					  i, first_block, ar_block, last_block );
			goto error_mount_phase2;
		}

		ar_block = le32_to_cpu( gdesc->bg_inode_table );

		if( ( ar_block < first_block ) || ( last_block < ar_block ) )
		{
			ME2FS_ERROR( "<ME2FS>error : block num of inode table is " );
			ME2FS_ERROR( "insanity [ group=%d, first=%lu, block=%lu, last=%lu ]\n",
					  i, first_block, ar_block, last_block );
			goto error_mount_phase2;
		}

		dbgPrintExt2Bgd( gdesc, i );
	}

	/* ------------------------------------------------------------------------ */
	/* initialize reservation window											*/
	/* ------------------------------------------------------------------------ */
	msi->s_rsv_window_head.rsv_start		= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
	msi->s_rsv_window_head.rsv_end			= EXT2_RESERVE_WINDOW_NOT_ALLOCATED;
	msi->s_rsv_window_head.rsv_alloc_hit	= 0;
	msi->s_rsv_window_head.rsv_goal_size	= 0;
	me2fsInsertReserveWindow( sb, &msi->s_rsv_window_head );

	/* ------------------------------------------------------------------------ */
	/* initialize exclusive locks												*/
	/* ------------------------------------------------------------------------ */
	spin_lock_init( &msi->s_rsv_window_lock );
	spin_lock_init( &msi->s_lock );

	bgl_lock_init( msi->s_blockgroup_lock );

	err = percpu_counter_init( &msi->s_freeblocks_counter,
							   me2fsCountFreeBlocks( sb ) );
	
	if( err )
	{
		ME2FS_ERROR( "<ME2FS>cannot allocate memory for percpu counter" );
		ME2FS_ERROR( "[s_freeblocks_counter]\n" );
		goto error_mount_phase3;
	}

	err = percpu_counter_init( &msi->s_freeinodes_counter,
							   me2fsCountFreeInodes( sb ) );
	
	if( err )
	{
		ME2FS_ERROR( "<ME2FS>cannot allocate memory for percpu counter" );
		ME2FS_ERROR( "[s_freeinodes_counter]\n" );
		goto error_mount_phase3;
	}

	err = percpu_counter_init( &msi->s_dirs_counter,
							   me2fsCountDirectories( sb ) );
	
	if( err )
	{
		ME2FS_ERROR( "<ME2FS>cannot allocate memory for percpu counter" );
		ME2FS_ERROR( "[s_dirs_counter]\n" );
		goto error_mount_phase3;
	}

	/* ------------------------------------------------------------------------ */
	/* add kobject to sysfs														*/
	/* ------------------------------------------------------------------------ */
	err = me2fsKobjInitAndAdd( sb, msi );

	if( err )
	{
		ME2FS_ERROR( "<ME2FS>cannot add kobject to sysfs\n" );
		goto error_mount_phase3;
	}

	/* ------------------------------------------------------------------------ */
	/* set up vfs super block													*/
	/* ------------------------------------------------------------------------ */
	sb->s_op		= &me2fs_super_ops;
	//sb->s_export_op	= &me2fs_export_ops;
	sb->s_xattr		= me2fs_xattr_handlers;
	sb->dq_op		= &dquot_operations;
	sb->s_qcop		= &dquot_quotactl_ops;

	sb->s_maxbytes	= me2fsMaxFileSize( sb );
	sb->s_max_links	= ME2FS_LINK_MAX;

	DBGPRINT( "<ME2FS>max file size = %lld\n", sb->s_maxbytes );


	root = me2fsGetVfsInode( sb, ME2FS_EXT2_ROOT_INO );
	//root = iget_locked( sb, ME2FS_EXT2_ROOT_INO );

	if( IS_ERR( root ) )
	{
		ME2FS_ERROR( "<ME2FS>error : failed to get root inode\n" );
		ret = PTR_ERR( root );
		goto error_mount_phase4;
	}


	if( !S_ISDIR( root->i_mode ) )
	{
		ME2FS_ERROR( "<ME2FS>root is not directory!!!!\n" );
	}

	sb->s_root = d_make_root( root );
	//sb->s_root = d_alloc_root( root_ino );

	if( !sb->s_root )
	{
		ME2FS_ERROR( "<ME2FS>error : failed to make root\n" );
		ret = -ENOMEM;
		goto error_mount_phase4;
	}

	le16_add_cpu( &esb->s_mnt_count, 1 );

	me2fsWriteSuper( sb );

	DBGPRINT( "<ME2FS> me2fs is mounted !\n" );

	return( 0 );

	/* ------------------------------------------------------------------------ */
	/* unregister kset															*/
	/* ------------------------------------------------------------------------ */
error_mount_phase4:
	me2fsKobjRemove( msi );
	/* ------------------------------------------------------------------------ */
	/* destroy percpu counter													*/
	/* ------------------------------------------------------------------------ */
error_mount_phase3:
	percpu_counter_destroy( &msi->s_freeblocks_counter );
	percpu_counter_destroy( &msi->s_freeinodes_counter );
	percpu_counter_destroy( &msi->s_dirs_counter );
	/* ------------------------------------------------------------------------ */
	/* release buffer for group descriptors										*/
	/* ------------------------------------------------------------------------ */
error_mount_phase2:
	for( i = 0 ; i < msi->s_gdb_count ; i++ )
	{
		brelse( msi->s_group_desc[ i ] );
	}
	kfree( msi->s_group_desc );
	/* ------------------------------------------------------------------------ */
	/* remove procfs entries													*/
	/* ------------------------------------------------------------------------ */
error_mount_proc:
	if( msi->s_proc )
	{
		remove_proc_entry( "options", msi->s_proc );
		remove_proc_entry( sb->s_id, me2fsGetProcRoot( ) );
	}
	/* ------------------------------------------------------------------------ */
	/* release buffer cache for read super block								*/
	/* ------------------------------------------------------------------------ */
error_mount:
	brelse( bh );

	/* ------------------------------------------------------------------------ */
	/* release me2fs super block information									*/
	/* ------------------------------------------------------------------------ */
error_read_sb:
	sb->s_fs_info = NULL;
	kfree( msi->s_blockgroup_lock );
	kfree( msi );

	return( ret );
}

/*
==================================================================================
	Function	:me2fsPutSuperBlock
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:put super block
==================================================================================
*/
static void me2fsPutSuperBlock( struct super_block *sb )
{
	struct me2fs_sb_info	*msi;
	int						i;

	dquot_disable( sb, -1, DQUOT_USAGE_ENABLED | DQUOT_LIMITS_ENABLED );

	msi = ME2FS_SB( sb );

	/* ------------------------------------------------------------------------ */
	/* destroy percpu counter													*/
	/* ------------------------------------------------------------------------ */
	percpu_counter_destroy( &msi->s_freeblocks_counter );
	percpu_counter_destroy( &msi->s_freeinodes_counter );
	percpu_counter_destroy( &msi->s_dirs_counter );

	/* ------------------------------------------------------------------------ */
	/* shrink mb cache															*/
	/* ------------------------------------------------------------------------ */
	me2fsXattrPutSuper( sb );

	/* ------------------------------------------------------------------------ */
	/* synchronize super block													*/
	/* ------------------------------------------------------------------------ */
	if( !( sb->s_flags & MS_RDONLY ) )
	{
		struct ext2_super_block		*esb;

		esb = msi->s_esb;

		spin_lock( &msi->s_lock );
		{
			esb->s_state = cpu_to_le16( msi->s_mount_state );
		}
		spin_unlock( &msi->s_lock );
		me2fsSyncSuper( sb, esb, 1 );
	}

	/* ------------------------------------------------------------------------ */
	/* remove entries from procfs												*/
	/* ------------------------------------------------------------------------ */
	if( msi->s_proc )
	{
		remove_proc_entry( "options", msi->s_proc );
		remove_proc_entry( sb->s_id, me2fsGetProcRoot( ) );
	}

	/* ------------------------------------------------------------------------ */
	/* remove entries from sysfs												*/
	/* ------------------------------------------------------------------------ */
	me2fsKobjRemove( msi );

	/* ------------------------------------------------------------------------ */
	/* release buffer cache for block group descriptor							*/
	/* ------------------------------------------------------------------------ */
	for( i = 0 ; i < msi->s_gdb_count ; i++ )
	{
		if( msi->s_group_desc[ i ] )
		{
			brelse( msi->s_group_desc[ i ] );
		}
	}

	kfree( msi->s_group_desc );

	/* ------------------------------------------------------------------------ */
	/* release buffer cache for super block										*/
	/* ------------------------------------------------------------------------ */
	brelse( msi->s_sbh );
	sb->s_fs_info = NULL;
	kfree( msi->s_blockgroup_lock );
	kfree( msi );
}

/*
==================================================================================
	Function	:dbgPrintExt2SB
	Input		:struct ext2_super_block *esb
				 < super block of ext2 file system >
	Output		:void
	Return		:void

	Description	:print ext2 super block
==================================================================================
*/
static void dbgPrintExt2SB( struct ext2_super_block *esb )
{
	unsigned int value;
	int i;
	value = ( unsigned int )( le32_to_cpu( esb->s_inodes_count ) );
	DBGPRINT( "<ME2FS>s_inode_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_blocks_count ) );
	DBGPRINT( "<ME2FS>s_blocks_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_r_blocks_count ) );
	DBGPRINT( "<ME2FS>s_r_blocks_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_free_blocks_count ) );
	DBGPRINT( "<ME2FS>s_free_blocks_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_free_inodes_count ) );
	DBGPRINT( "<ME2FS>s_free_inodes_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_first_data_block ) );
	DBGPRINT( "<ME2FS>s_first_data_block = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_log_block_size ) );
	DBGPRINT( "<ME2FS>s_log_block_size = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_log_frag_size ) );
	DBGPRINT( "<ME2FS>s_log_frag_size = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_blocks_per_group ) );
	DBGPRINT( "<ME2FS>s_blocks_per_group = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_frags_per_group ) );
	DBGPRINT( "<ME2FS>s_frags_per_group = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_inodes_per_group ) );
	DBGPRINT( "<ME2FS>s_inodes_per_group = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_mtime ) );
	DBGPRINT( "<ME2FS>s_mtime = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_wtime ) );
	DBGPRINT( "<ME2FS>s_wtime = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_mnt_count ) );
	DBGPRINT( "<ME2FS>s_mnt_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_max_mnt_count ) );
	DBGPRINT( "<ME2FS>s_max_mnt_count = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_magic ) );
	DBGPRINT( "<ME2FS>s_magic = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_state ) );
	DBGPRINT( "<ME2FS>s_state = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_errors ) );
	DBGPRINT( "<ME2FS>s_errors = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_minor_rev_level ) );
	DBGPRINT( "<ME2FS>s_minor_rev_level = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_lastcheck ) );
	DBGPRINT( "<ME2FS>s_lastcheck = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_creator_os ) );
	DBGPRINT( "<ME2FS>s_creator_os = %u\n", value );
	value = ( unsigned int )( le16_to_cpu( esb->s_rev_level ) );
	DBGPRINT( "<ME2FS>s_rev_level = %u\n", value );
	value = ( unsigned int )( le16_to_cpu( esb->s_def_resuid ) );
	DBGPRINT( "<ME2FS>s_def_resuid = %u\n", value );
	value = ( unsigned int )( le16_to_cpu( esb->s_def_resgid ) );
	DBGPRINT( "<ME2FS>s_def_resgid = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_first_ino ) );
	DBGPRINT( "<ME2FS>s_first_ino = %u\n", value );
	value = ( unsigned int )( le16_to_cpu( esb->s_inode_size ) );
	DBGPRINT( "<ME2FS>s_inode_size = %u\n", value );
	value = ( unsigned int )( le16_to_cpu( esb->s_block_group_nr ) );
	DBGPRINT( "<ME2FS>s_block_group_nr = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_feature_compat ) );
	DBGPRINT( "<ME2FS>s_feature_compat = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_feature_incompat ) );
	DBGPRINT( "<ME2FS>s_feature_incompat = %u\n", value );
	DBGPRINT( "<ME2FS>s_uuid[ 16 ] = " );
	for( i = 0 ; i < 16 ; i++ )
	{
		DBGPRINT( "%02X", esb->s_uuid[ i ] );
	}
	DBGPRINT( "\n" );
	value = ( unsigned int )( le32_to_cpu( esb->s_journal_inum ) );
	DBGPRINT( "<ME2FS>s_journal_inum = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_journal_dev ) );
	DBGPRINT( "<ME2FS>s_journal_dev = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_last_orphan ) );
	DBGPRINT( "<ME2FS>s_last_orphan = %u\n", value );
	DBGPRINT( "<ME2FS>s_hash_seed[ 4 ] = " );
	for( i = 0 ; i < 4 ; i++ )
	{
		DBGPRINT( "%16X", esb->s_hash_seed[ i ] );
	}
	DBGPRINT( "\n" );
	value = ( unsigned int )( esb->s_def_hash_version );
	DBGPRINT( "<ME2FS>s_def_hash_version = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_default_mount_opts ) );
	DBGPRINT( "<ME2FS>s_default_mount_opts = %u\n", value );
	value = ( unsigned int )( le32_to_cpu( esb->s_first_meta_bg ) );
	DBGPRINT( "<ME2FS>s_first_meta_bg = %u\n", value );
}

/*
==================================================================================
	Function	:dbgPrintExt2Bgd
	Input		:struct ext2_group_desc *group_desc
				 < ext2 group descriptor >
				 unsigned long group_no
				 < group number to print information >
	Output		:void
	Return		:void

	Description	:print block group descriptor
==================================================================================
*/
static void
dbgPrintExt2Bgd( struct ext2_group_desc *group_desc, unsigned long group_no )
{
	unsigned int			value;

	DBGPRINT( "<ME2FS>:[group %lu]\n", group_no );
	value = le32_to_cpu( group_desc->bg_block_bitmap );
	DBGPRINT( "<ME2FS>:bg_block_bitmap = %u\n", value );
	value = le32_to_cpu( group_desc->bg_inode_bitmap );
	DBGPRINT( "<ME2FS>:bg_inode_bitmap = %u\n", value );
	value = le32_to_cpu( group_desc->bg_inode_table );
	DBGPRINT( "<ME2FS>:bg_inode_table = %u\n", value );
	value = le32_to_cpu( group_desc->bg_free_blocks_count );
	DBGPRINT( "<ME2FS>:bg_free_blocks_count = %u\n", value );
	value = le32_to_cpu( group_desc->bg_free_inodes_count );
	DBGPRINT( "<ME2FS>:bg_free_inodes_count = %u\n", value );
	value = le32_to_cpu( group_desc->bg_used_dirs_count );
	DBGPRINT( "<ME2FS>:bg_used_dirs_count = %u\n", value );
}

/*
==================================================================================
	Function	:dbgPrintMe2fsInfo
	Input		:struct me2fs_sb_info *msi
				 < me2fs super block information >
	Output		:void
	Return		:void

	Description	:print me2fs information
==================================================================================
*/
static void dbgPrintMe2fsInfo( struct me2fs_sb_info *msi )
{
	DBGPRINT( "<ME2FS>msi->s_sb_block=%lu\n", msi->s_sb_block );
	DBGPRINT( "<ME2FS>msi->s_mount_opt=%lu\n", msi->s_mount_opt );
	DBGPRINT( "<ME2FS>msi->s_mount_state=%d\n", msi->s_mount_state );
	DBGPRINT( "<ME2FS>msi->s_inode_size=%d\n", msi->s_inode_size );
	DBGPRINT( "<ME2FS>msi->s_first_ino=%d\n", msi->s_first_ino );
	DBGPRINT( "<ME2FS>msi->s_inodes_per_group=%lu\n", msi->s_inodes_per_group );
	DBGPRINT( "<ME2FS>msi->s_inodes_per_block=%lu\n", msi->s_inodes_per_block );
	DBGPRINT( "<ME2FS>msi->s_groups_count=%lu\n", msi->s_groups_count );
	DBGPRINT( "<ME2FS>msi->s_blocks_per_group=%lu\n", msi->s_blocks_per_group );
	DBGPRINT( "<ME2FS>msi->s_desc_per_block=%lu\n", msi->s_desc_per_block );
	DBGPRINT( "<ME2FS>msi->s_gdb_count=%lu\n", msi->s_gdb_count );
	DBGPRINT( "<ME2FS>msi->s_frag_size=%lu\n", msi->s_frag_size );
	DBGPRINT( "<ME2FS>msi->s_frags_per_block=%lu\n", msi->s_frags_per_block );
	DBGPRINT( "<ME2FS>msi->s_frags_per_group=%lu\n", msi->s_frags_per_group );
	DBGPRINT( "<ME2FS>msi->s_resuid=%u\n", msi->s_resuid.val );
	DBGPRINT( "<ME2FS>msi->s_resgid=%u\n", msi->s_resgid.val );
}
/*
==================================================================================
	Function	:me2fsAllocInode
	Input		:struct super_block *sb
				 < vsf super block object >
	Output		:void
	Return		:struct inode*
				 < allocated inode of vfs >

	Description	:allocate memory to vfs inode and me2fs inode information
==================================================================================
*/
static struct inode *me2fsAllocInode( struct super_block *sb )
{
	struct me2fs_inode_info	*mi;

	mi = ( struct me2fs_inode_info * )kmem_cache_alloc( me2fs_inode_cachep,
														GFP_KERNEL );

	if( !mi )
	{
		return( NULL );
	}

	mi->vfs_inode.i_version = 1;

	return( &mi->vfs_inode );
}

/*
==================================================================================
	Function	:me2fsDestroyInode
	Input		:struct inode *inode
				 < vfs inode to destroy >
	Output		:void
	Return		:void

	Description	:destroy vfs inode and me2fs inode information
==================================================================================
*/
static void me2fsDestroyInode( struct inode *inode )
{
	struct me2fs_inode_info *ei = ME2FS_I( inode );
	kmem_cache_free( me2fs_inode_cachep, ei );
}

/*
==================================================================================
	Function	:me2fsInitInodeOnce
	Input		:void *object
				 < object allocated by slab allocator >
	Output		:void
	Return		:void

	Description	:initialize vfs inode and me2fs inode information
				 callbacked by slab allocator
==================================================================================
*/
static void me2fsInitInodeOnce( void *object )
{
	struct me2fs_inode_info *ei = ( struct me2fs_inode_info* )object;

	/* ------------------------------------------------------------------------ */
	/* initialize locks															*/
	/* ------------------------------------------------------------------------ */
	rwlock_init( &ei->i_meta_lock );
	mutex_init( &ei->truncate_mutex );
	init_rwsem( &ei->xattr_sem );

	/* ------------------------------------------------------------------------ */
	/* initialize vfs inode														*/
	/* ------------------------------------------------------------------------ */
	inode_init_once( &ei->vfs_inode );
}

/*
==================================================================================
	Function	:me2fsMaxFileSize
	Input		:struct super_block *sb
				 < vfs super bock >
	Output		:void
	Return		:loff_t
				 < result of max file size >

	Description	:calculate max file size
==================================================================================
*/
static loff_t me2fsMaxFileSize( struct super_block *sb )
{
	int		file_blocks;
	int		nr_blocks;
	loff_t	max_size;

	/* direct blocks															*/
	file_blocks = ME2FS_NDIR_BLOCKS;
	/* indirect blocks															*/
	nr_blocks = sb->s_blocksize / sizeof( u32 );
	file_blocks += nr_blocks;
	/* double indirect blocks													*/
	file_blocks += nr_blocks * nr_blocks;
	/* tripple indiret blocks													*/
	file_blocks += nr_blocks * nr_blocks * nr_blocks;

	max_size = file_blocks * sb->s_blocksize;

	if( MAX_LFS_FILESIZE < max_size )
	{
		max_size = MAX_LFS_FILESIZE;
	}

	return( max_size );
}
/*
==================================================================================
	Function	:getDescriptorLocation
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned long logic_sb_block
				 < logical super block number >
				 int num_gb
				 < block count allocated to block group descriptors >
	Output		:void
	Return		:unsigned long
				 < first block of group descriptor >

	Description	:get block number of group descriptor
==================================================================================
*/
static unsigned long
getDescriptorLocation( struct super_block *sb,
					   unsigned long logic_sb_block,
					   int num_bg )
{
	struct me2fs_sb_info	*msi;
	unsigned long			bg;
	unsigned long			first_meta_bg;
	int						has_super;
	
	msi				= ME2FS_SB( sb );
	has_super		= 0;
	first_meta_bg	= le32_to_cpu( msi->s_esb->s_first_meta_bg );

	if( ( msi->s_esb->s_feature_incompat &
		  cpu_to_le32( EXT2_FEATURE_INCOMPAT_META_BG ) ) ||
		( num_bg < first_meta_bg ) )
	{
		return( logic_sb_block + num_bg + 1 );
	}

	bg = msi->s_desc_per_block * num_bg;

	if( me2fsHasBgSuper( sb, bg ) )
	{
		has_super = 1;
	}

	return( ext2GetFirstBlockNum( sb, bg ) + has_super );
}

/*
==================================================================================
	Function	:me2fsSyncFs
	Input		:struct super_block *sb
				 < vfs super block >
				 int wait
				 < blocking flag >
	Output		:void
	Return		:int
				 < result >

	Description	:synchronize filesystem
==================================================================================
*/
static int me2fsSyncFs( struct super_block *sb, int wait )
{
	struct me2fs_sb_info	*msi;
	struct ext2_super_block	*esb;

	DBGPRINT( "<ME2FS>%s:sync_super\n", __func__ );

	/* ------------------------------------------------------------------------ */
	/* write quota structures to quota file, sync_blockdev() will write them to	*/
	/* disk later																*/
	/* ------------------------------------------------------------------------ */
	dquot_writeback_dquots( sb, -1 );

	msi = ME2FS_SB( sb );
	esb = msi->s_esb;

	spin_lock( &msi->s_lock );

	if( esb->s_state & cpu_to_le16( EXT2_VALID_FS ) )
	{
		DBGPRINT( "<ME2FS>%s:debug:setting s_state to 0.\n", __func__ );
		esb->s_state &= cpu_to_le16( ~EXT2_VALID_FS );
	}
	
	spin_unlock( &msi->s_lock );

	me2fsSyncSuper( sb, esb, wait );

	return( 0 );

}
/*
==================================================================================
	Function	:me2fsSyncSuper
	Input		:struct super_block *sb
				 < vfs super block >
				 struct ext2_super_block *esb
				 < buffer cache address of ext2 super block >
				 int wait
				 < blocking flag >
	Output		:void
	Return		:void

	Description	:synchronize super block
==================================================================================
*/
static void me2fsSyncSuper( struct super_block *sb,
							struct ext2_super_block *esb,
							int wait )
{
	struct me2fs_sb_info	*msi;

	msi = ME2FS_SB( sb );

	clearSuperError( sb );
	spin_lock( &msi->s_lock );
	esb->s_free_blocks_count = cpu_to_le32( me2fsCountFreeBlocks( sb ) );
	esb->s_free_inodes_count = cpu_to_le32( me2fsCountFreeInodes( sb ) );
	/* unlock before i/o														*/
	spin_unlock( &msi->s_lock );
	mark_buffer_dirty( msi->s_sbh );
	if( wait )
	{
		sync_dirty_buffer( msi->s_sbh );
	}
}
/*
==================================================================================
	Function	:clearSuperError
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:clear write i/o error of buffer cache
==================================================================================
*/
static void clearSuperError( struct super_block *sb )
{
	struct buffer_head	*sbh;

	sbh = ME2FS_SB( sb )->s_sbh;

	if( buffer_write_io_error( sbh ) )
	{
		/* -------------------------------------------------------------------- */
		/* a previous attempt to write the superblock failed. this could happen	*/
		/* because the usb device was yanked out. or it could happen to be		*/
		/* a transient write error and maybe the block will be remapped.		*/
		/* nothing we can do but to retry the write and hope for the best.		*/
		/* -------------------------------------------------------------------- */
		ME2FS_ERROR( "<ME2FS>previous i/o erro to super block detected\n" );
		clear_buffer_write_io_error( sbh );
		set_buffer_uptodate( sbh );
	}
}
/*
==================================================================================
	Function	:me2fsStatfs
	Input		:struct dentry *dentry
				 < file path >
				 struct kstatfs *buf
				 < buffer to store information >
	Output		:void
	Return		:void

	Description	:get information about the filesystem
==================================================================================
*/
static int me2fsStatfs( struct dentry *dentry, struct kstatfs *buf )
{
	struct super_block		*sb;
	struct me2fs_sb_info	*msi;
	struct ext2_super_block	*esb;
	u64						fsid;

	sb	= dentry->d_sb;
	msi	= ME2FS_SB( sb );
	esb	= msi->s_esb;

	spin_lock( &msi->s_lock );
	{
		if( msi->s_blocks_last != le32_to_cpu( esb->s_blocks_count ) )
		{
			unsigned long	i;
			unsigned long	overhead;

			overhead = 0;

			smp_rmb( );

			/* ---------------------------------------------------------------- */
			/* compute the overhead (fs structures). this is constant for a		*/
			/* given filesystem unless the number of block groups changes so	*/
			/* we cache the previous value until it does.						*/
			/* ---------------------------------------------------------------- */

			/* ---------------------------------------------------------------- */
			/* all of the blocks before first_data_block are overhead			*/
			/* ---------------------------------------------------------------- */
			overhead = le32_to_cpu( esb->s_first_data_block );

			/* ---------------------------------------------------------------- */
			/* add the overhead attributed to the superblock and block group	*/
			/* descriptors. if the sparse superblocks feature is turned on,		*/
			/* then not all groups have this.									*/
			/* ---------------------------------------------------------------- */
			for( i = 0 ; i < msi->s_groups_count ; i++ )
			{
				overhead += me2fsHasBgSuper( sb, i ) +
							me2fsGetBlocksUsedByGroupTable( sb, i );
			}

			/* ---------------------------------------------------------------- */
			/* every block group has an inode btimap a block bitmap, and an		*/
			/* inode table														*/
			/* ---------------------------------------------------------------- */
			overhead += ( msi->s_groups_count * ( 2 + msi->s_itb_per_group ) );
			msi->s_overhead_last = overhead;
			smp_wmb( );
			msi->s_blocks_last = le32_to_cpu( esb->s_blocks_count );
		}

		buf->f_type		= ME2FS_SUPER_MAGIC;
		buf->f_bsize	= sb->s_blocksize;
		buf->f_blocks	= le32_to_cpu( esb->s_blocks_count )
						  - msi->s_overhead_last;
		buf->f_bfree	= me2fsCountFreeBlocks( sb );
		
		esb->s_free_blocks_count = cpu_to_le32( buf->f_bfree );
		buf->f_bavail	= buf->f_bfree - le32_to_cpu( esb->s_r_blocks_count );
		if( buf->f_bfree < le32_to_cpu( esb->s_r_blocks_count ) )
		{
			buf->f_bavail = 0;
		}
		buf->f_files	= le32_to_cpu( esb->s_inodes_count );
		buf->f_ffree	= me2fsCountFreeInodes( sb );
		buf->f_namelen	= ME2FS_NAME_LEN;
		fsid = le64_to_cpup( ( void* )esb->s_uuid ) ^
			   le64_to_cpup( ( void* )esb->s_uuid + sizeof( u64 ) );
		buf->f_fsid.val[ 0 ] = fsid & 0xFFFFFFFFUL;
		buf->f_fsid.val[ 1 ] = ( fsid >> 32 ) & 0xFFFFFFFFUL;
	}
	spin_unlock( &msi->s_lock );

	return( 0 );
}
/*
==================================================================================
	Function	:me2fsRemount
	Input		:struct super_block *sb
				 < vfs super block >
				 int *flags
				 < mount flags >
				 char *data
				 < mount options >
	Output		:void
	Return		:int
				 < result >

	Description	:remount filesystem
==================================================================================
*/
static int me2fsRemount( struct super_block *sb,
						 int *flags,
						 char *data )
{
	struct me2fs_mount_options
	{
		unsigned long	s_mount_opt;
		kuid_t			s_resuid;
		kgid_t			s_resgid;
	};
	
	struct me2fs_sb_info		*msi;
	struct ext2_super_block		*esb;
	unsigned long				old_mount_opt;
	struct me2fs_mount_options	old_opts;
	unsigned long				old_sb_flags;
	int							err;

	DBGPRINT( "<ME2FS>Remount me2fs!!!!\n" );

	msi				= ME2FS_SB( sb );
	old_mount_opt	= msi->s_mount_opt;

	sync_filesystem( sb );

	spin_lock( &msi->s_lock );
	/* ------------------------------------------------------------------------ */
	/* store the old options													*/
	/* ------------------------------------------------------------------------ */
	old_sb_flags			= sb->s_flags;
	old_opts.s_mount_opt	= msi->s_mount_opt;
	old_opts.s_resuid		= msi->s_resuid;
	old_opts.s_resgid		= msi->s_resgid;

	/* ------------------------------------------------------------------------ */
	/* allow the "check" option to be passed as a remount option				*/
	/* ------------------------------------------------------------------------ */
	if( !parseOptions( data, sb ) )
	{
		err = -EINVAL;
		goto restore_opts;
	}

	if( msi->s_mount_opt & EXT2_MOUNT_POSIX_ACL )
	{
		sb->s_flags = sb->s_flags | MS_POSIXACL;
	}
	else
	{
		sb->s_flags = sb->s_flags & ~MS_POSIXACL;
	}

	esb = msi->s_esb;

	if( ( *flags & MS_RDONLY ) == ( sb->s_flags & MS_RDONLY ) )
	{
		spin_unlock( &msi->s_lock );
		return( 0 );
	}

	if( *flags & MS_RDONLY )
	{
		if( ( le16_to_cpu( esb->s_state ) & EXT2_VALID_FS ) ||
			!( msi->s_mount_state & EXT2_VALID_FS ) )
		{
			spin_unlock( &msi->s_lock );
			return( 0 );
		}
		/* -------------------------------------------------------------------- */
		/* we are remounting a valid rw partition rdonly, so set the rdonly		*/
		/* flag and then mark the partition as valid again						*/
		/* -------------------------------------------------------------------- */
		esb->s_state	= cpu_to_le16( msi->s_mount_state );
		esb->s_mtime	= cpu_to_le32( get_seconds( ) );
		spin_unlock( &msi->s_lock );

		if( ( err = dquot_suspend( sb, -1 ) ) < 0 )
		{
			spin_lock( &msi->s_lock );
			goto restore_opts;
		}

		me2fsSyncSuper( sb, esb, 1 );
	}
	else
	{
		__le32	ret;
		
		ret = msi->s_esb->s_feature_ro_compat &
			  cpu_to_le32( ~EXT2_FEATURE_RO_COMPAT_SUPP );
		
		if( ret )
		{
			ME2FS_ERROR( "<ME2FS>%s:warning:couldn't remout RDWR "
						 "because of unsupported optional features (%x).",
						 __func__, le32_to_cpu( ret ) );
			err = -EROFS;
			goto restore_opts;
		}
		/* -------------------------------------------------------------------- */
		/* mounting a RDONLY partition read-write, so reread and store the		*/
		/* current valid flag. (it may have been changed by e2fsck since we		*/
		/* originally mounted the partition										*/
		/* -------------------------------------------------------------------- */
		msi->s_mount_state = le16_to_cpu( esb->s_state );
#if 0	// options
		if( !setupSuper( sb, esb, 0 ) )
		{
			sb->s_flags &= ~MS_RDONLY;
		}
#endif
		spin_unlock( &msi->s_lock );

		me2fsWriteSuper( sb );

		dquot_resume( sb, -1 );
	}

	return( 0 );

restore_opts:
	msi->s_mount_opt	= old_opts.s_mount_opt;
	msi->s_resuid		= old_opts.s_resuid;
	msi->s_resgid		= old_opts.s_resgid;
	sb->s_flags			= old_sb_flags;
	spin_unlock( &msi->s_lock );

	return( err );
}
/*
==================================================================================
	Function	:parseOptions
	Input		:char *options
				 < options string >
				 struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:void

	Description	:parse mount options
==================================================================================
*/
static int parseOptions( char *options, struct super_block *sb )
{
	char					*substring;
	struct me2fs_sb_info	*msi;
	substring_t				args[ MAX_OPT_ARGS ];			// MAX_OPT_ARGS = 3
	int						option;
	kuid_t					uid;
	kgid_t					gid;

	DBGPRINT( "<ME2FS>parse options! %s\n", options );

	if( !options )
	{
		return( 1 );
	}

	msi = ME2FS_SB( sb );

	while( ( substring = strsep( &options ,"," ) ) != NULL )
	{
		int	token;

		if( substring == NULL )
		{
			continue;
		}

		token = match_token( substring, tokens, args );

		switch( token )
		{
		case	Opt_bsd_df:
			msi->s_mount_opt &= ~EXT2_MOUNT_MINIX_DF;
			DBGPRINT( "<ME2FS>option:bsd_df is always on:"
					  "clear minix_df flag\n" );
			break;
		case	Opt_minix_df:
			/* i don't implement minixdf										*/
			DBGPRINT( "<ME2FS>option:minix_df:bsd_df flag is always on\n" );
			break;
		case	Opt_grpid:
			msi->s_mount_opt |= EXT2_MOUNT_GRPID;
			DBGPRINT( "<ME2FS>option:grpid:set grpid flag\n" );
			break;
		case	Opt_nogrpid:
			msi->s_mount_opt &= ~EXT2_MOUNT_GRPID;
			DBGPRINT( "<ME2FS>option:nogrpid:clear grpid flag\n" );
			break;
		case	Opt_resuid:
			if( match_int( &args[ 0 ], &option ) )
			{
				/* substring is not decimal representaion of an integer			*/
				DBGPRINT( "<ME2FS>option:resuid:invalid parameter\n" );
				return( 0 );
			}
			uid = make_kuid( current_user_ns( ), option );
			if( !uid_valid( uid ) )
			{
				ME2FS_ERROR( "<ME2FS>%s:error:invalid uid %d\n",
							 __func__, option );
				return( 0 );
			}
			msi->s_resuid = uid;
			DBGPRINT( "<ME2FS>option:resuid:uid is %d\n", option );
			break;
		case	Opt_resgid:
			if( match_int( &args[ 0 ], &option ) )
			{
				DBGPRINT( "<ME2FS>option:resgid:invalid parameter\n" );
				return( 0 );
			}
			gid = make_kgid( current_user_ns( ), option );
			if( !gid_valid( gid ) )
			{
				ME2FS_ERROR( "<ME2FS>%s:error:invalid gid %d\n",
							 __func__, option );
				return( 0 );
			}
			msi->s_resgid = gid;
			DBGPRINT( "<ME2FS>option:resgid:gid is %d\n", option );
			break;
		case	Opt_sb:
			/* handled by getSbBlock( ) instead here							*/
			break;
		case	Opt_err_panic:
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_CONT;
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_RO;
			msi->s_mount_opt |=  EXT2_MOUNT_ERRORS_PANIC;
			DBGPRINT( "<ME2FS>option:clear error_cont, error_ro,"
					  " set error_panic\n" );
			break;
		case	Opt_err_ro:
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_CONT;
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_PANIC;
			msi->s_mount_opt |=  EXT2_MOUNT_ERRORS_RO;
			DBGPRINT( "<ME2FS>option:clear error_cont, error_panic,"
					  " set error_ro\n" );
			break;
		case	Opt_err_cont:
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_RO;
			msi->s_mount_opt &= ~EXT2_MOUNT_ERRORS_PANIC;
			msi->s_mount_opt |=  EXT2_MOUNT_ERRORS_CONT;
			DBGPRINT( "<ME2FS>option:clear error_ro, error_panic,"
					  " set error_cont\n" );
			break;
		case	Opt_nouid32:
			msi->s_mount_opt |=  EXT2_MOUNT_NO_UID32;
			DBGPRINT( "<ME2FS>option:set nouid32 flag\n" );
			break;
		case	Opt_nocheck:
			msi->s_mount_opt &= ~EXT2_MOUNT_CHECK;
			DBGPRINT( "<ME2FS>option:clear check flag\n" );
			break;
		case	Opt_debug:
			msi->s_mount_opt |=  EXT2_MOUNT_DEBUG;
			DBGPRINT( "<ME2FS>option:set debug flag\n" );
			break;
		case	Opt_oldalloc:
		case	Opt_orlov:
			/* i implement only orlov method									*/
			msi->s_mount_opt &= ~EXT2_MOUNT_OLDALLOC;
			DBGPRINT( "<ME2FS>option:always orlov method is on\n" );
			break;
		case	Opt_nobh:
			msi->s_mount_opt |=  EXT2_MOUNT_NOBH;
			DBGPRINT( "<ME2FS>option:set nobh flag\n" );
			break;
		case	Opt_user_xattr:
			msi->s_mount_opt |=  EXT2_MOUNT_XATTR_USER;
			DBGPRINT( "<ME2FS>option:set xattr_user flag\n" );
			break;
		case	Opt_nouser_xattr:
			msi->s_mount_opt &= ~EXT2_MOUNT_XATTR_USER;
			DBGPRINT( "<ME2FS>option:clear xattr_user flag\n" );
			break;
		case	Opt_acl:
			msi->s_mount_opt |=  EXT2_MOUNT_POSIX_ACL;
			DBGPRINT( "<ME2FS>option:set posix_acl flag\n" );
			break;
		case	Opt_noacl:
			msi->s_mount_opt &= ~EXT2_MOUNT_POSIX_ACL;
			DBGPRINT( "<ME2FS>option:clear posix_acl flag\n" );
			break;
		case	Opt_quota:
		case	Opt_usrquota:
			msi->s_mount_opt |=  EXT2_MOUNT_USRQUOTA;
			DBGPRINT( "<ME2FS>option:set usrquota flag\n" );
			break;
		case	Opt_grpquota:
			msi->s_mount_opt |=  EXT2_MOUNT_GRPQUOTA;
			DBGPRINT( "<ME2FS>option:set grpquota flag\n" );
			break;
		case	Opt_noreservation:
			ME2FS_ERROR( "<ME2FS>reservation is always on!!\n" );
		case	Opt_reservation:
			msi->s_mount_opt |=  EXT2_MOUNT_RESERVATION;
			break;
		case	Opt_ignore:
			DBGPRINT( "<ME2FS>option:ignore...\n" );
			break;
		default:
			DBGPRINT( "<ME2FS>option:unkonwn option\n" );
			return( 0 );
		}
	}
	
	return( 1 );
}
/*
==================================================================================
	Function	:getSbBlock
	Input		:void **data
				 < mount options >
	Output		:void
	Return		:void

	Description	:get block number of super block from mount options
==================================================================================
*/
static unsigned long getSbBlock( void **data )
{
	unsigned long	sb_block;
	char			*options;

	options = ( char* )*data;

	if( !options || ( strncmp( options, "sb=", sizeof( "sb=" ) - 1 ) != 0 ) )
	{
		/* default location														*/
		return( 1 );
	}

	options += sizeof( "sb=" ) - 1;

	sb_block = simple_strtoul( options, &options, 0 );

	if( *options && ( *options != ',' ) )
	{
		ME2FS_ERROR( "<ME2FS>invalid sb spec:%s\n", ( char* )data );
		return( 1 );
	}

	if( *options == ',' )
	{
		options++;
	}

	*data = ( void* )options;

	return( sb_block );
}
/*
==================================================================================
	Function	:me2fsShowOptions
	Input		:struct seq_file *seq
				 < sequential file for proc fs >
				 struct dentry *root
				 < path of root >
	Output		:void
	Return		:int
				 < result >

	Description	:show options for proc fs
==================================================================================
*/
static int me2fsShowOptions( struct seq_file *seq, struct dentry *root )
{
	struct super_block		*sb;
	struct me2fs_sb_info	*msi;
	struct ext2_super_block	*esb;
	unsigned long			def_mount_opts;

	DBGPRINT( "<me2fs>show options\n" );

	sb	= root->d_sb;
	msi	= ME2FS_SB( sb );
	esb	= msi->s_esb;

	spin_lock( &msi->s_lock );
	{
		def_mount_opts = le32_to_cpu( esb->s_default_mount_opts );

		if( msi->s_sb_block != 1 )
		{
			seq_printf( seq, ",sb=%lu", msi->s_sb_block );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_MINIX_DF )
		{
			seq_printf( seq, ",minixdf" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_GRPID )
		{
			seq_printf( seq, ",grpid" );
		}
		if( !( msi->s_mount_opt & EXT2_MOUNT_GRPID ) &&
			( def_mount_opts & EXT2_DEFM_BSDGROUPS ) )
		{
			seq_printf( seq, ",nogrpid" );
		}
		if( !gid_eq( msi->s_resgid,
					 make_kgid( &init_user_ns, EXT2_DEF_RESUID ) ) ||
			le16_to_cpu( esb->s_def_resuid ) != EXT2_DEF_RESUID )
		{
			seq_printf( seq,
						",resgid=%u",
						from_kgid_munged( &init_user_ns, msi->s_resgid ) );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_ERRORS_RO )
		{
			int	def_errors;

			def_errors = le16_to_cpu( esb->s_errors );

			if( ( def_errors == EXT2_ERRORS_PANIC ) ||
				( def_errors == EXT2_ERRORS_CONTINUE ) )
			{
				seq_printf( seq, ",errors=remount-ro" );
			}
		}
		if( msi->s_mount_opt & EXT2_MOUNT_ERRORS_CONT )
		{
			seq_printf( seq, ",errors=continue" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_ERRORS_PANIC )
		{
			seq_printf( seq, ",errors=panic" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_NO_UID32 )
		{
			seq_printf( seq, ",nouid32" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_DEBUG )
		{
			seq_printf( seq, ",debug" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_XATTR_USER )
		{
			seq_printf( seq, ",user_xattr" );
		}
		if( !( msi->s_mount_opt & EXT2_MOUNT_XATTR_USER ) &&
			( def_mount_opts & EXT2_DEFM_XATTR_USER ) )
		{
			seq_printf( seq, ",nouser_xattr" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_POSIX_ACL )
		{
			seq_printf( seq, ",acl" );
		}
		if( !( msi->s_mount_opt & EXT2_MOUNT_POSIX_ACL ) &&
			( def_mount_opts & EXT2_DEFM_ACL ) )
		{
			seq_printf( seq, ",noacl" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_USRQUOTA )
		{
			seq_printf( seq, ",usrquota" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_GRPQUOTA )
		{
			seq_printf( seq, ",grpquota" );
		}
		if( msi->s_mount_opt & EXT2_MOUNT_XIP )
		{
			seq_printf( seq, ",xip" );
		}
		if( !( msi->s_mount_opt & EXT2_MOUNT_RESERVATION ) )
		{
			seq_printf( seq, ",noreservation" );
		}
	}
	spin_unlock( &msi->s_lock );

	return( 0 );
}
/*
==================================================================================
	Function	:me2fsFreeze
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:int
				 < result >

	Description	:freeze filesyste
==================================================================================
*/
static int me2fsFreeze( struct super_block *sb )
{
	struct me2fs_sb_info	*msi;

	DBGPRINT( "<ME2FS>freeze filesystem\n" );

	if( atomic_long_read( &sb->s_remove_count ) )
	{
		me2fsSyncFs( sb, 1 );
		return( 0 );
	}

	msi = ME2FS_SB( sb );

	/* ------------------------------------------------------------------------ */
	/* set EXT2_FS_VALID flag(s_mount_state has VALID flag)						*/
	/* ------------------------------------------------------------------------ */
	spin_lock( &msi->s_lock );
	{
		msi->s_esb->s_state = cpu_to_le16( msi->s_mount_state );
	}
	spin_unlock( &msi->s_lock );

	me2fsSyncSuper( sb, msi->s_esb, 1 );

	return( 0 );
}
/*
==================================================================================
	Function	:me2fsUnfreeze
	Input		:struct super_block *sb
				 < vfs super block >
	Output		:void
	Return		:int
				 < result >

	Description	:unfreeze filesystem
==================================================================================
*/
static int me2fsUnfreeze( struct super_block *sb )
{
	DBGPRINT( "<ME2FS>unfreeze filesystem\n" );

	me2fsWriteSuper( sb );

	return( 0 );
}

/*
==================================================================================
	Function	:me2fsQuotaRead
	Input		:struct super_block *sb
				 < vfs super block >
				 int type
				 < type of quota >
				 char *data
				 < quota data buffer >
				 size_t len
				 < length of buffer >
				 loff_t off
				 < offset >
	Output		:void
	Return		:ssize_t
				< read size >

	Description	:read data from quotafile
==================================================================================
*/
static ssize_t
me2fsQuotaRead( struct super_block *sb,
				int type,
				char *data,
				size_t len,
				loff_t off )
{
	struct inode		*inode;
	sector_t			blk;
	int					err;
	int					offset;
	int					tocopy;
	size_t				toread;
	struct buffer_head	tmp_bh;
	struct buffer_head	*bh;
	loff_t				i_size;

	inode	= sb_dqopt( sb )->files[ type ];
	i_size	= i_size_read( inode );

	if( i_size < off )
	{
		return( 0 );
	}

	if( i_size < ( off + len ) )
	{
		len = i_size - off;
	}

	blk		= off >> sb->s_blocksize_bits;
	err		= 0;
	offset	= off & ( sb->s_blocksize - 1 );

	toread	= len;

	while( 0 < toread )
	{
		if( ( sb->s_blocksize - offset ) < toread )
		{
			tocopy = sb->s_blocksize - offset;
		}
		else
		{
			tocopy = toread;
		}

		tmp_bh.b_state	= 0;
		tmp_bh.b_size	= sb->s_blocksize;

		err = me2fsGetBlock( inode, blk, &tmp_bh, 0 );

		if( err < 0 )
		{
			return( err );
		}

		/* a hole ?																*/
		if( !buffer_mapped( &tmp_bh ) )
		{
			memset( data, 0, tocopy );
		}
		else
		{
			if( !( bh = sb_bread( sb, tmp_bh.b_blocknr ) ) )
			{
				return( -EIO );
			}

			memcpy( data, bh->b_data + offset, tocopy );
			brelse( bh );
		}

		offset	= 0;
		toread	-= tocopy;
		data	+= tocopy;
		blk++;
	}

	return( len );
}
/*
==================================================================================
	Function	:me2fsQuotaWrite
	Input		:struct super_block *sb
				 < vfs super block >
				 int type
				 < type of quota >
				 const char *data
				 < quota data >
				 size_t len
				 < length of data >
				 loff_t off
				 < offset to write >
	Output		:void
	Return		:ssize_t
				 < size to write >

	Description	:write to quota file
==================================================================================
*/
static ssize_t
me2fsQuotaWrite( struct super_block *sb,
				 int type,
				 const char *data,
				 size_t len,
				 loff_t off )
{
	struct inode		*inode;
	sector_t			blk;
	int					err;
	int					offset;
	int					tocopy;
	size_t				towrite;
	struct buffer_head	tmp_bh;
	struct buffer_head	*bh;

	inode	= sb_dqopt( sb )->files[ type ];
	blk		= off >> sb->s_blocksize_bits;
	err		= 0;
	offset	= off & ( sb->s_blocksize - 1 );
	towrite	= len;

	while( 0 < towrite )
	{
		if( ( sb->s_blocksize - offset ) < towrite )
		{
			tocopy = sb->s_blocksize - offset;
		}
		else
		{
			tocopy = towrite;
		}

		tmp_bh.b_state	= 0;
		tmp_bh.b_size	= sb->s_blocksize;
		err = me2fsGetBlock( inode, blk, &tmp_bh, 1 );
		if( err < 0 )
		{
			goto out;
		}

		if( offset || ( tocopy != sb->s_blocksize ) )
		{
			bh = sb_bread( sb, tmp_bh.b_blocknr );
		}
		else
		{
			bh = sb_getblk( sb, tmp_bh.b_blocknr );
		}

		if( unlikely( !bh ) )
		{
			err = -EIO;
			goto out;
		}

		lock_buffer( bh );
		{
			memcpy( bh->b_data + offset, data, tocopy );
			flush_dcache_page( bh->b_page );
			set_buffer_uptodate( bh );
			mark_buffer_dirty( bh );
		}
		unlock_buffer( bh );
		brelse( bh );
		offset	= 0;
		towrite	-= tocopy;
		data	+= tocopy;
		blk++;
	}

out:
	if( len == towrite )
	{
		return( err );
	}

	if( inode->i_size < ( off + len - towrite ) )
	{
		i_size_write( inode, off + len - towrite );
	}

	inode->i_version++;
	inode->i_mtime = CURRENT_TIME;
	inode->i_ctime = inode->i_mtime;
	mark_inode_dirty( inode );
	
	return( len - towrite );
}
/*
----------------------------------------------------------------------------------
	seq options file operations
----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:optionsOpenFs
	Input		:struct inode *inode
				 < inode object of procfs >
				 struct file *file
				 < file object of procfs >
	Output		:void
	Return		:int
				 < result >

	Description	:open method of seq options file operation
==================================================================================
*/
static int optionsOpenFs( struct inode *inode, struct file *file )
{
	return( single_open( file, optionsSeqShow, PDE_DATA( inode ) ) );
}

/*
==================================================================================
	Function	:optionsSeqShow
	Input		:struct seq_file *seq
				 < seq file >
				 void *offset
				 < offset in a file >
	Output		:void
	Return		:int
				 < result >

	Description	:options seq show
==================================================================================
*/
static int optionsSeqShow( struct seq_file *seq, void *offset )
{
	struct super_block	*sb;
	int					rc;

	sb = seq->private;

	if( sb->s_flags & MS_RDONLY )
	{
		seq_puts( seq, "ro");
	}
	else
	{
		seq_puts( seq, "rw");
	}

	rc = me2fsShowOptions( seq, sb->s_root );
	seq_puts( seq, "\n" );

	return( rc );
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


