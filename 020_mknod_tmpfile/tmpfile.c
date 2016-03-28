#include <stdio.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>
#include <fcntl.h>

#ifndef O_TMPFILE
#define O_TMPFILE 020000000
#endif


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

	Description	:make a temporary file example
_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
*/
int main( int argc, char *argv[ ] )
{
	int		tmp_fd;
	int		read_len;
	char	buf[ 256 ];

	if( argc != 2 )
	{
		fprintf( stderr, "usage:%s dir_path\n", argv[ 0 ] );
	}

	/* ------------------------------------------------------------------------ */
	/* create a temporary file													*/
	/* ------------------------------------------------------------------------ */
	//if( ( tmp_fd = open( argv[ 1 ], O_RDWR | O_TMPFILE, 0660 ) ) < 0 )
	if( ( tmp_fd = open( argv[ 1 ], O_RDWR | O_TMPFILE | O_DIRECTORY, 0660 ) ) < 0 )
	{
		perror( "error : open " );
		fprintf( stderr, "mak sure that %s is directory path\n", argv[ 1 ] );
		return( tmp_fd );
	}

	printf( "a temporary file is created."
			"To kill this process, please press any key.\n" );
	
	read_len = read( STDIN_FILENO, buf, sizeof( buf ) );

	if( read_len < 0 )
	{
		perror( "error : read " );
	}

	// NOTE : i do not delete the temporary file

	return( 0 );
}
