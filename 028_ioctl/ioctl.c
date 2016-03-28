/********************************************************************************
	File			: ioctl.c
	Description		: set i_generation and get i_generation

*********************************************************************************/
#include <stdio.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <linux/fs.h>

/*
==================================================================================

	Prototype Statement

==================================================================================
*/
int Close( int fd );

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
	Function	:main
	Input		:int argc
				 < number of arguments >
				 char *argv[ ]
				 < arguments >
	Output		:void
	Return		:int
				 < result >

	Description	:set i_generation and get i_generation
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int main( int argc, char *argv[ ] )
{
	char 	*path;
	int		fd;
	int		err;
	int		version = 100;

	if( argc != 2 )
	{
		fprintf( stderr, "usage : %s path\n", argv[ 0 ] );
		return( -1 );
	}

	path = argv[ 1 ];

	if( ( fd = open( path, O_RDWR ) ) < 0 )
	{
		perror( "open : " );
		return( -1 );
	}

	if( ( err = ioctl( fd, FS_IOC_SETVERSION, &version ) ) < 0 )
	{
		perror( "ioctl : " );
		goto close_fd;
	}

	printf( "set i_generation = %d\n", version );

	if( ( err = ioctl( fd, FS_IOC_GETVERSION, &version ) ) < 0 )
	{
		perror( "ioctl : " );
		goto close_fd;
	}

	printf( "get i_generation = %d\n", version );

close_fd:
	return( Close( fd ) );

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
	Function	:Close
	Input		:int fd
				 < file descriptor to close >
	Output		:void
	Return		:int
				 < result >

	Description	:close a file
==================================================================================
*/
int Close( int fd )
{
	int	err;

	if( ( err = close( fd ) ) < 0 )
	{
		perror( "close : " );
	}

	return( err );
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
