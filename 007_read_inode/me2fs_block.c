/********************************************************************************
	File			: me2fs_block.c
	Description		: management blocks of my ext2 file system

*********************************************************************************/

#include "me2fs.h"
#include "me2fs_util.h"


/*
==================================================================================

	Prototype Statement

==================================================================================
*/
static int isGroupSparse( int group );
static int testRoot( int group, int multiple );

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
	Function	:me2fsGetGroupDescriptor
	Input		:struct super_block *sb
				 < vfs super block >
				 unsigned int block_group
				 < block group number >
	Output		:void
	Return		:struct ext2_group_desc*
				 < goup descriptor >

	Description	:get ext2 group descriptor
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
struct ext2_group_desc*
me2fsGetGroupDescriptor( struct super_block *sb,
						unsigned int block_group )
{
	struct me2fs_sb_info	*msi;
	unsigned long			gdesc_index;
	unsigned long			gdesc_offset;
	struct ext2_group_desc	*group_desc;

	msi = ME2FS_SB( sb );

	if( msi->s_groups_count <= block_group )
	{
		ME2FS_ERROR( "<ME2FS>block group number is out of groups count of sb\n" );
		ME2FS_ERROR( "<ME2FS>s_groups_count = %lu\n", msi->s_groups_count );
		ME2FS_ERROR( "<ME2FS>block_group = %u\n", block_group );
		return( NULL );
	}

	gdesc_index		= block_group / msi->s_desc_per_block;

	if( !( group_desc =
		( struct ext2_group_desc* )( msi->s_group_desc[ gdesc_index ]->b_data )  ) )
	{
		ME2FS_ERROR( "<ME2FS>canot find %u th group descriptor\n", block_group );
		return( NULL );
	}

	gdesc_offset	= block_group % msi->s_desc_per_block;

	//return( &group_desc[ gdesc_offset ] );
	return( group_desc + gdesc_offset );
}
/*
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
	Function	:me2fsHasBgSuper
	Input		:struct super_block *sb
				 < vfs super block >
				 int gourp
				 < group number >
	Output		:void
	Return		:int
				 < result >

	Description	:does block goup have super block?
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int me2fsHasBgSuper( struct super_block *sb, int group )
{
	if( ( ME2FS_SB( sb )->s_esb->s_feature_ro_compat &
		  EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER ) &&
		( !isGroupSparse( group ) ) )
	{
		return( 0 );
	}

	return( 1 );
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
	Function	:isGroupSparse
	Input		:int group
				 < group number >
	Output		:void
	Return		:int
				 < result >

	Description	:is group sparsed?
==================================================================================
*/
static int isGroupSparse( int group )
{
	if( group <= 1 )
	{
		return( 1 );
	}

	return( testRoot( group, 3 ) ||
			testRoot( group, 5 ) ||
			testRoot( group, 7 ) );
}

/*
==================================================================================
	Function	:testRoot
	Input		:int group
				 < group number >
				 int multiple
				 < multiple number >
	Output		:void
	Return		:void

	Description	:test whether given group number is multiples of given
				 multiple number or not
==================================================================================
*/
static int testRoot( int group, int multiple )
{
	int		num;

	num = multiple;

	while( num < group )
	{
		num *= multiple;
	}

	return( num == group );
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
