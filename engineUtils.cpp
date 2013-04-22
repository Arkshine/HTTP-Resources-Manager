
#include "engineUtils.h"
#include "engineCvars.h"
#include "engineConfigs.h"

void SZ_Clear( sizebuf_t *buf ) 
{
	buf->cursize = 0;
	buf->overflow = 0;
}

void* SZ_GetSpace( sizebuf_t* buf, uint32 length ) 
{
	void* data;

	if( buf->cursize + length > buf->maxsize ) 
	{
		if( !*buf->debugname )
		{
			strcpy( buf->debugname, "???" );
		}

		if( !( buf->overflow & 1 ) )
		{
			if( length )
				printf/*Sys_Error*/( "SZ_GetSpace: overflow without FSB_ALLOWOVERFLOW set on %s", buf->debugname );
			else
				printf/*Sys_Error*/( "SZ_GetSpace:  Tried to write to an uninitialized sizebuf_t: %s", buf->debugname );

			exit( EXIT_FAILURE );
		}

		if( length > buf->maxsize )
		{
			if( buf->overflow & 1 )
			{
				if( developer->value > 0 )
					printf/*Con_DPrintf*/( "SZ_GetSpace: %i is > full buffer size on %s, ignoring", length, buf->debugname );
			}
			else
			{
				printf/*Sys_Error*/( "SZ_GetSpace: %i is > full buffer size on %s", length, buf->debugname );
				exit( EXIT_FAILURE );
			}

		}

		printf( "SZ_GetSpace: overflow on %s\n", buf->debugname );

		SZ_Clear( buf );
		buf->overflow |= 2;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return( data );
}

void SZ_Write( sizebuf_t *buf, const void *data, int length ) 
{
	memcpy( SZ_GetSpace( buf, length ), data, length );
}

void MSG_WriteByte( sizebuf_t *sb, uint32 c ) 
{
	byte*	buf;

	buf = ( byte* )SZ_GetSpace( sb, 1 );
	buf[ 0 ] = ( byte )c;
}

void MSG_WriteLong( sizebuf_t *sb, uint32 c ) 
{
	byte*	buf;

	buf = ( byte* )SZ_GetSpace( sb, 4 );
	*( unsigned long* )buf = c;
}

void MSG_WriteShort( sizebuf_t *sb, int c ) 
{
	byte    *buf;

	buf = ( byte* )SZ_GetSpace( sb, 2 );
	*( word* )buf = ( word )c;
}

void MSG_WriteString( sizebuf_t *sb, const char *s ) 
{
	if( s == NULL )
		SZ_Write( sb, "", 1 );
	else 
		SZ_Write( sb, s, strlen( s ) + 1 );
}

void MSG_WriteBitString( const char* str ) 
{
	const char* ptr;

	for( ptr = str; *ptr != '\0'; ptr++ ) 
	{
		MSG_WriteBits( *ptr, 8 );
	}

	MSG_WriteBits( 0, 8 );
}

void MSG_WriteBitData( unsigned char* str, int size ) 
{
	int i;
	unsigned char* ptr;

	for( i = 0, ptr = str; i < size; i++, ptr++ ) 
	{
		MSG_WriteBits( *ptr, 8 );
	}
}