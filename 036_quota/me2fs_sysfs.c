/********************************************************************************
	File			: me2fs_sysfs.c
	Description		: sysfs operations for my ext2 file system

*********************************************************************************/
#include <linux/completion.h>

#include "me2fs.h"
#include "me2fs_util.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static void releaseKobj( struct kobject *kobj );
/*
----------------------------------------------------------------------------------
	Attribute methods of me2fs Superblock information
----------------------------------------------------------------------------------
*/
static ssize_t ulShow( struct kobject *kobj, struct attribute *attr, char *buf );
static ssize_t uiShow( struct kobject *kobj, struct attribute *attr, char *buf );
static ssize_t usShow( struct kobject *kobj, struct attribute *attr, char *buf );
static ssize_t uxShow( struct kobject *kobj, struct attribute *attr, char *buf );

/*
----------------------------------------------------------------------------------
	Attribute methods of me2fs Superblock information
----------------------------------------------------------------------------------
*/
static ssize_t le32Show( struct kobject *kobj,
						 struct attribute *attr, char *buf );
static ssize_t le16Show( struct kobject *kobj,
						 struct attribute *attr, char *buf );
static ssize_t le32XShow( struct kobject *kobj,
						  struct attribute *attr, char *buf );
static ssize_t stringShow( struct kobject *kobj,
						   struct attribute *attr, char *buf );
static ssize_t uuidShow( struct kobject *kobj,
						 struct attribute *attr, char *buf );

/*
==================================================================================

	DEFINES

==================================================================================
*/
struct me2fs_attr
{
	struct attribute	attr;
	ssize_t ( *show )( struct kobject *kobj, struct attribute *attr, char *buf );
	ssize_t ( *store )( struct kobject *kobj,
						struct attribute *attr,
						const char *buf );
	int					offset;
};

#define	ATTR_LIST( name )	&me2fs_attr_##name.attr

/*
----------------------------------------------------------------------------------
	Attribute of me2fs Superblock information
----------------------------------------------------------------------------------
*/
#define	ME2FS_ATTR_OFFSET( _name, _mode, _show, _store, _elname )				\
static struct me2fs_attr me2fs_attr_##_name = {									\
	.attr	= { .name = __stringify( _name ), .mode = _mode },					\
	.show	= _show,															\
	.store	= _store,															\
	.offset	= offsetof( struct me2fs_sb_info, _elname ),						\
}

#define	ME2FS_MI_UL_ATTR( name )												\
ME2FS_ATTR_OFFSET( name, 0444, ulShow, NULL, s_##name )
#define	ME2FS_MI_UI_ATTR( name )												\
ME2FS_ATTR_OFFSET( name, 0444, uiShow, NULL, s_##name )
#define	ME2FS_MI_US_ATTR( name )												\
ME2FS_ATTR_OFFSET( name, 0444, usShow, NULL, s_##name )
#define	ME2FS_MI_UX_ATTR( name )												\
ME2FS_ATTR_OFFSET( name, 0444, uxShow, NULL, s_##name )


/*
----------------------------------------------------------------------------------
	Attribute of ext2 superblock
----------------------------------------------------------------------------------
*/
#define	EXT2_ATTR_OFFSET( _name, _mode, _show, _store, _elname )				\
static struct me2fs_attr me2fs_attr_##_name = {									\
	.attr	= { .name = __stringify( _name ), .mode = _mode },					\
	.show	= _show,															\
	.store	= _store,															\
	.offset	= offsetof( struct ext2_super_block, _elname ),						\
}

#define	ME2FS_ES_LE32_ATTR( name )												\
EXT2_ATTR_OFFSET( name, 0444, le32Show, NULL,   s_##name )
#define	ME2FS_ES_LE16_ATTR( name )												\
EXT2_ATTR_OFFSET( name, 0444, le16Show, NULL,   s_##name )
#define	ME2FS_ES_LE32X_ATTR( name )												\
EXT2_ATTR_OFFSET( name, 0444, le32XShow, NULL,  s_##name )
#define	ME2FS_ES_STRING_ATTR( name )											\
EXT2_ATTR_OFFSET( name, 0444, stringShow, NULL, s_##name )
#define	ME2FS_ES_UUID_ATTR( name )												\
EXT2_ATTR_OFFSET( name, 0444, uuidShow, NULL,   s_##name )


/*
==================================================================================

	Management

==================================================================================
*/
struct kset		*me2fs_kset;

/* me2fs superblock information													*/
ME2FS_MI_UL_ATTR( sb_block );
ME2FS_MI_UX_ATTR( mount_opt );
ME2FS_MI_US_ATTR( mount_state );
ME2FS_MI_US_ATTR( inode_size );
ME2FS_MI_UI_ATTR( first_ino );
ME2FS_MI_UL_ATTR( inodes_per_group );
ME2FS_MI_UL_ATTR( inodes_per_block );
ME2FS_MI_UL_ATTR( itb_per_group );
ME2FS_MI_UL_ATTR( groups_count );
ME2FS_MI_UL_ATTR( blocks_per_group );
ME2FS_MI_UL_ATTR( desc_per_block );
ME2FS_MI_UL_ATTR( gdb_count );
ME2FS_MI_UL_ATTR( frag_size );
ME2FS_MI_UL_ATTR( frags_per_block );
ME2FS_MI_UL_ATTR( frags_per_group );
ME2FS_MI_UI_ATTR( resuid );
ME2FS_MI_UI_ATTR( resgid );

/* ext2 superblock																*/
ME2FS_ES_LE32_ATTR( inodes_count );
ME2FS_ES_LE32_ATTR( blocks_count );
ME2FS_ES_LE32_ATTR( r_blocks_count );
ME2FS_ES_LE32_ATTR( free_blocks_count );
ME2FS_ES_LE32_ATTR( free_inodes_count );
ME2FS_ES_LE32_ATTR( first_data_block );
ME2FS_ES_LE32_ATTR( log_block_size );
ME2FS_ES_LE32_ATTR( log_frag_size );
ME2FS_ES_LE32_ATTR( mtime );
ME2FS_ES_LE32_ATTR( wtime );
ME2FS_ES_LE16_ATTR( mnt_count );
ME2FS_ES_LE16_ATTR( max_mnt_count );
ME2FS_ES_LE16_ATTR( state );
ME2FS_ES_LE16_ATTR( errors );
ME2FS_ES_LE16_ATTR( minor_rev_level );
ME2FS_ES_LE32_ATTR( lastcheck );
ME2FS_ES_LE32_ATTR( checkinterval );
ME2FS_ES_LE32_ATTR( creator_os );
ME2FS_ES_LE16_ATTR( rev_level );
ME2FS_ES_LE16_ATTR( def_resuid );
ME2FS_ES_LE16_ATTR( def_resgid );
ME2FS_ES_LE32X_ATTR( feature_compat );
ME2FS_ES_LE32X_ATTR( feature_incompat );
ME2FS_ES_LE32X_ATTR( feature_ro_compat );
ME2FS_ES_UUID_ATTR( uuid );
ME2FS_ES_STRING_ATTR( volume_name );
ME2FS_ES_STRING_ATTR( last_mounted );
ME2FS_ES_LE32X_ATTR( algorithm_usage_bitmap );
ME2FS_ES_LE32_ATTR( prealloc_blocks );
ME2FS_ES_LE32_ATTR( prealloc_dir_blocks );

static struct attribute *me2fs_attrs[ ] =
{
	/* me2fs superblock information												*/
	ATTR_LIST( sb_block ),
	ATTR_LIST( mount_opt ),
	ATTR_LIST( mount_state ),
	ATTR_LIST( inode_size ),
	ATTR_LIST( first_ino ),
	ATTR_LIST( inodes_per_group ),
	ATTR_LIST( inodes_per_block ),
	ATTR_LIST( itb_per_group ),
	ATTR_LIST( groups_count ),
	ATTR_LIST( blocks_per_group ),
	ATTR_LIST( desc_per_block ),
	ATTR_LIST( gdb_count ),
	ATTR_LIST( frag_size ),
	ATTR_LIST( frags_per_block ),
	ATTR_LIST( frags_per_group ),
	ATTR_LIST( resuid ),
	ATTR_LIST( resgid ),
	/* ext2 superblock															*/
	ATTR_LIST( inodes_count ),
	ATTR_LIST( blocks_count ),
	ATTR_LIST( r_blocks_count ),
	ATTR_LIST( free_blocks_count ),
	ATTR_LIST( free_inodes_count ),
	ATTR_LIST( first_data_block ),
	ATTR_LIST( log_block_size ),
	ATTR_LIST( log_frag_size ),
	ATTR_LIST( mtime ),
	ATTR_LIST( wtime ),
	ATTR_LIST( mnt_count ),
	ATTR_LIST( max_mnt_count ),
	ATTR_LIST( state ),
	ATTR_LIST( errors ),
	ATTR_LIST( minor_rev_level ),
	ATTR_LIST( lastcheck ),
	ATTR_LIST( checkinterval ),
	ATTR_LIST( creator_os ),
	ATTR_LIST( rev_level ),
	ATTR_LIST( def_resuid ),
	ATTR_LIST( def_resgid ),
	ATTR_LIST( feature_compat ),
	ATTR_LIST( feature_incompat ),
	ATTR_LIST( feature_ro_compat ),
	ATTR_LIST( uuid ),
	ATTR_LIST( volume_name ),
	ATTR_LIST( last_mounted ),
	ATTR_LIST( algorithm_usage_bitmap ),
	ATTR_LIST( prealloc_blocks ),
	ATTR_LIST( prealloc_dir_blocks ),
	NULL,
};

static struct kobj_type me2fs_ktype =
{
	.default_attrs	= me2fs_attrs,
	.sysfs_ops		= &kobj_sysfs_ops,
	.release		= releaseKobj,
};

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
int me2fsInitKset( void )
{
	me2fs_kset = kset_create_and_add( "me2fs", NULL, fs_kobj );

	if( !me2fs_kset )
	{
		return( -ENOMEM );
	}

	return( 0 );
}

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
						 struct me2fs_sb_info *msi )
{
	int error;

	msi->s_kobj.kset = me2fs_kset;
	
	error = kobject_init_and_add( &msi->s_kobj,
								  &me2fs_ktype, &me2fs_kset->kobj, "%s", sb->s_id );
	
	if( !error )
	{
		init_completion( &msi->s_kobj_unregister );
	}
	
	return( error );
}

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
void me2fsKobjDel( struct me2fs_sb_info *msi )
{
	kobject_del( &msi->s_kobj );
}

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
void me2fsKobjPut( struct me2fs_sb_info *msi )
{
	kobject_put( &msi->s_kobj );
}

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
void me2fsKobjRemove( struct me2fs_sb_info *msi )
{
	me2fsKobjDel( msi );
	me2fsKobjPut( msi );
	wait_for_completion( &msi->s_kobj_unregister );
}


/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsUnregisterKset
	Input		:void
	Output		:void
	Return		:void

	Description	:unregister kset from sysfs
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
void me2fsUnregisterKset( void )
{
	DBGPRINT( "<ME2FS>kset unregister\n" );
	kset_unregister( me2fs_kset );
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
	Function	:releaseKobj
	Input		:struct kobject *kobj
				 < general object >
	Output		:void
	Return		:void

	Description	:release kobject
==================================================================================
*/
static void releaseKobj( struct kobject *kobj )
{
	struct me2fs_sb_info	*msi;

	msi = container_of( kobj, struct me2fs_sb_info, s_kobj );

	complete( &msi->s_kobj_unregister );
}

/*
----------------------------------------------------------------------------------
	Attribute methods of me2fs Superblock information
----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:ulShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for unsigned long
==================================================================================
*/
static ssize_t ulShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	unsigned long			*ul;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	ul		= ( unsigned long* )( ( ( char* )mi ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%lu\n", *ul ) );
}

/*
==================================================================================
	Function	:uiShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for unsigned int
==================================================================================
*/
static ssize_t uiShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	unsigned int			*ui;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	ui		= ( unsigned int* )( ( ( char* )mi ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%u\n", *ui ) );
}

/*
==================================================================================
	Function	:usShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for unsigned short
==================================================================================
*/
static ssize_t usShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	unsigned short			*us;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	us		= ( unsigned short* )( ( ( char* )mi ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%u\n", *us ) );
}

/*
==================================================================================
	Function	:uxShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for hexadecimal  of unsigned int
==================================================================================
*/
static ssize_t uxShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	unsigned int			*ui;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	ui		= ( unsigned int* )( ( ( char* )mi ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%08X\n", *ui ) );
}


/*
----------------------------------------------------------------------------------
	Attribute methods of me2fs Superblock information
----------------------------------------------------------------------------------
*/
/*
==================================================================================
	Function	:le32Show
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for __le32
==================================================================================
*/
static ssize_t le32Show( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	__le32					*es_le32;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	es_le32	= ( unsigned int* )( ( ( char* )mi->s_esb ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%u\n", le32_to_cpu( *es_le32 ) ) );
}

/*
==================================================================================
	Function	:le16Show
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for __le16
==================================================================================
*/
static ssize_t le16Show( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	__le16					*es_le16;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	es_le16	= ( __le16* )( ( ( char* )mi->s_esb ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%u\n", le16_to_cpu( *es_le16 ) ) );
}

/*
==================================================================================
	Function	:le32XShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for hexadecimal format of __le32
==================================================================================
*/
static ssize_t le32XShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	__le32					*es_le32;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	es_le32	= ( __le32* )( ( ( char* )mi->s_esb ) + me_attr->offset );

	return( scnprintf( buf, PAGE_SIZE, "%08X\n", le32_to_cpu( *es_le32 ) ) );
}

/*
==================================================================================
	Function	:stringShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for string
==================================================================================
*/
static ssize_t stringShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	char					*string;
	ssize_t					size;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	string	= ( ( char* )mi->s_esb ) + me_attr->offset;

	size = scnprintf( buf, PAGE_SIZE - 1, "%s\n", string );

	buf[ size++ ] = '\n';

	return( size );
}


/*
==================================================================================
	Function	:uuidShow
	Input		:struct kobject *kobj
				 < general object >
				 struct attribute *attr
				 < general attribute >
				 char *buf
				 < buffer to output >
	Output		:void
	Return		:ssize_t
				 < actual output size >

	Description	:show method for uuid
==================================================================================
*/
static ssize_t uuidShow( struct kobject *kobj, struct attribute *attr, char *buf )
{
	struct me2fs_sb_info	*mi;
	struct me2fs_attr		*me_attr;
	char					*uuid;
	int						i;
	ssize_t					size = 0;

	mi		= container_of( kobj, struct me2fs_sb_info, s_kobj );
	me_attr	= container_of( attr, struct me2fs_attr, attr );

	uuid	= ( ( char* )mi->s_esb ) + me_attr->offset;

	for( i = 0 ; i < 16 ; i++ )
	{
		size += scnprintf( buf + ( i * 2 ),
						   PAGE_SIZE - ( i * 2 ) - 1, "%02X", uuid[ i ] );
	}

	buf[ size++ ] = '\n';

	return( size );
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
