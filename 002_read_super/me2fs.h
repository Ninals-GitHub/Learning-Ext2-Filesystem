/********************************************************************************
	File			: me2fs.h
	Description		: Defines for my ext2 file system

********************************************************************************/
#ifndef	__ME2FS_H__
#define	__ME2FS_H__

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
---------------------------------------------------------------------------------
	Ext2 Super Block
---------------------------------------------------------------------------------
*/
struct ext2_super_block
{
	__le32	s_inodes_count;
	__le32	s_blocks_count;
	__le32	s_r_blocks_count;				/* reserved blocks count			*/
	__le32	s_free_blocks_count;
	__le32	s_free_inodes_count;
	__le32	s_first_data_block;
	__le32	s_log_block_size;
	__le32	s_log_frag_size;
	__le32	s_blocks_per_group;
	__le32	s_frags_per_group;
	__le32	s_inodes_per_group;
	__le32	s_mtime;						/* mount time						*/
	__le32	s_wtime;						/* write time						*/
	__le16	s_mnt_count;
	__le16	s_max_mnt_count;
	__le16	s_magic;
	__le16	s_state;						/* file system state				*/
	__le16	s_errors;						/* behaviour when detecting error	*/
	__le16	s_minor_rev_level;				/* minor revision level				*/
	__le32	s_lastcheck;					/* time of last check				*/
	__le32	s_checkinterval;				/* max. time between checks			*/
	__le32	s_creator_os;
	__le16	s_rev_level;					/* revision level					*/
	__le16	s_def_resuid;					/* default uid for reserved blocks	*/
	__le16	s_def_resgid;					/* default gid for reserved blocks	*/
	/* ------------------------------------------------------------------------ */
	/* Dynamic Revision															*/
	/* ------------------------------------------------------------------------ */
	__le32	s_first_ino;					/* first non-reserved inode			*/
	__le16	s_inode_size;					/* size of inode structure			*/
	__le16	s_block_group_nr;				/* block group # of this superblock	*/
	__le32	s_feature_compat;				/* compatible feature set			*/
	__le32	s_feature_incompat;				/* incompatible feature set			*/
	__le32	s_feature_ro_compat;			/* readonly-compatible feature set	*/
	__u8	s_uuid[ 16 ];					/* 128-bit uuid for volume			*/
	char	s_volume_name[ 16 ];			/* volume name						*/
	char	s_last_mounted[ 64 ];			/* directory where last mounted		*/
	__le32	s_algorithm_usage_bitmap;		/* For compression					*/
	/* ------------------------------------------------------------------------ */
	/* Peformance Hints															*/
	/* ------------------------------------------------------------------------ */
	__u8	s_prealloc_blocks;				/* # of blocks to try to preallocate*/
	__u8	s_prealloc_dir_blocks;			/* # of preaalocat for dirs			*/
	__u16	s_padding1;
	/* ------------------------------------------------------------------------ */
	/* Journaling Support														*/
	/* ------------------------------------------------------------------------ */
	__u8	s_journal_uuid[ 16 ];			/* uuid of journal superblock		*/
	__u32	s_journal_inum;					/* inode number of journal file		*/
	__u32	s_journal_dev;					/* device number of journal file	*/
	__u32	s_last_orphan;					/* start of list of inodes to delete*/
	/* ------------------------------------------------------------------------ */
	/* Directory Indexing Support												*/
	/* ------------------------------------------------------------------------ */
	__u32	s_hash_seed[ 4 ];				/* HTREE hash seed					*/
	__u8	s_def_hash_version;				/* default hash version				*/
	__u8	s_reserved_char_pad;
	__u16	s_reserved_word_pad;
	/* ------------------------------------------------------------------------ */
	/* Other options															*/
	/* ------------------------------------------------------------------------ */
	__le32	s_default_mount_opts;
	__le32	s_first_meta_bg;				/* first metablock block group		*/
	__u32	s_reserved[ 190 ];				/* padding to the end				*/

};

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
#endif	// __ME2FS_H__
