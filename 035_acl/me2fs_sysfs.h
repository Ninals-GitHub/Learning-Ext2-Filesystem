/*********************************************************************************
	File			: me2fs_sysfs.h
	Description		: Definitions of me2fs sysfs operations

*********************************************************************************/
#ifndef	__ME2FS_SYSFS_H__
#define	__ME2FS_SYSFS_H__


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
	Function	:me2fsInitKset
	Input		:void
	Output		:void
	Return		:int
				 < result >

	Description	:initialize me2fs kset
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsInitKset( void );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsKobjInitAndAdd
	Input		:struct super_block *sb
				 < vfs superblock >
				 struct me2fs_sb_info *msi
				 < superblock inromation >
	Output		:void
	Return		:int
				 < result >

	Description	:initialize me2fs kobject and add it to sysfs
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsKobjInitAndAdd( struct super_block *sb,
						 struct me2fs_sb_info *msi );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsKobjDel
	Input		:struct me2fs_sb_info *msi
				 < me2fs superblock information >
	Output		:void
	Return		:void

	Description	:delete kobject
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsKobjDel( struct me2fs_sb_info *msi );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsKobjPut
	Input		:struct me2fs_sb_info *msi
				 < me2fs superblock information >
	Output		:void
	Return		:void

	Description	:put kobject
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsKobjPut( struct me2fs_sb_info *msi );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsKobjRemove
	Input		:struct me2fs_sb_info *msi
				 < me2fs superblock information >
	Output		:void
	Return		:void

	Description	:remove kobject and wait
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsKobjRemove( struct me2fs_sb_info *msi );

/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsUnregisterKset
	Input		:void
	Output		:void
	Return		:void

	Description	:unregister kset from sysfs
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsUnregisterKset( void );

#endif	// __ME2FS_SYSFS_H__
