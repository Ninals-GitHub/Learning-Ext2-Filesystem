/********************************************************************************
	File			: me2fs_super.c
	Description		: super block operations for my ext2 file sytem

********************************************************************************/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>

#include "me2fs.h"
#include "me2fs_util.h"


/*
=================================================================================

	Prototype Statement

=================================================================================
*/
static int me2fsFillSuperBlock( struct super_block *sb,
								void *data,
								int silent );

static void dbgPrintExt2SB( struct ext2_super_block *esb );
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
	int						block_size;
	int						ret = -EINVAL;
	unsigned long			sb_block = 1;

	/* ------------------------------------------------------------------------ */
	/* set device's block size to super block									*/
	/* ------------------------------------------------------------------------ */
	block_size = sb_min_blocksize( sb, BLOCK_SIZE );

	DBGPRINT( "<ME2FS>Fill Super! block_size = %d\n", block_size );
	DBGPRINT( "<ME2FS>default block size is : %d\n", BLOCK_SIZE );

	if( !block_size )
	{
		DBGPRINT( "<ME2FS>error: unable to set blocksize\n" );
		return( ret );
	}

	if( !( bh = sb_bread( sb, 1 ) ) )
	{
		DBGPRINT( "<ME2FS>failed to bread super block\n" );
		return( ret );
	}

	esb = ( struct ext2_super_block* )( bh->b_data );

	dbgPrintExt2SB( esb );

	return( 0 );
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
	Function	:void
	Input		:void
	Output		:void
	Return		:void

	Description	:void
==================================================================================
*/

