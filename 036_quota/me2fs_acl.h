/*********************************************************************************
	File			: me2fs_acl.h
	Description		: Definition for Posix ACL

*********************************************************************************/
#ifndef	__ME2FS_POSIX_ACL_H__
#define	__ME2FS_POSIX_ACL_H__


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
#define	EXT2_ACL_VERSION		0x0001
/*
----------------------------------------------------------------------------------
	Acl header
----------------------------------------------------------------------------------
*/
struct ext2_acl_header
{
	__le32		a_version;
};
/*
----------------------------------------------------------------------------------
	Acl entry
----------------------------------------------------------------------------------
*/
struct ext2_acl_entry
{
	__le16		e_tag;
	__le16		e_perm;
	__le32		e_id;
};

/*
----------------------------------------------------------------------------------
	Acl entry short
----------------------------------------------------------------------------------
*/
struct ext2_acl_entry_short
{
	__le16		e_tag;
	__le16		e_perm;
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
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsGetAcl
	Input		:struct inode *inode
				 < vfs inode >
				 int type
				 < type of acl >
	Output		:void
	Return		:struct posix_acl*
				 < return posix acl >

	Description	:get posix acl
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct posix_acl*
me2fsGetAcl( struct inode *inode, int type );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsSetAcl
	Input		:struct inode *inode
				 < vfs inode >
				 struct posix_acl *acl
				 < posix acl >
				 int type
				 < acl type >
	Output		:void
	Return		:int
				 < result >

	Description	:set posix acl
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsSetAcl( struct inode *inode, struct posix_acl *acl, int type );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitAcl
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *dir
				 < vfs inode of parent directory >
	Output		:void
	Return		:int
				 < result >

	Description	:initialize the acls of a new inode.
				 called from me2fsAllocNewInode
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitAcl( struct inode *inode, struct inode *dir );


#endif	// __ME2FS_POSIX_ACL_H__
