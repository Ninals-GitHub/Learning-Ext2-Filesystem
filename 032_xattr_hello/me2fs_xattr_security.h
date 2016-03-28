/*********************************************************************************
	File			: me2fs_xattr_security.h
	Description		: Definition for Security Extended Attribute Handler

*********************************************************************************/
#ifndef	__ME2FS_XATTR_SECURITY_H__
#define	__ME2FS_XATTR_SECURITY_H__


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
extern const struct xattr_handler me2fs_xattr_security_handler;

/*
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

	< Open Functions >

++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsInitSecurity
	Input		:struct inode *inode
				 < vfs inode >
				 struct inode *dir
				 < vfs inode of parent directory >
				 const struct qstr *qstr
				 < name >
	Output		:void
	Return		:void

	Description	:initialize security
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitSecurity( struct inode *inode,
					   struct inode *dir,
					   const struct qstr *qstr );

#endif	// __ME2FS_XATTR_SECURITY_H__
