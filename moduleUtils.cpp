
#include "moduleUtils.h"
#include "moduleConfigs.h"

void retrieveModName( String* modName )
{
	char gameDir[ 512 ];
	GET_GAME_DIR( gameDir );

	const char* a = gameDir;

	int i = 0;

	while( gameDir[ i ] )
	{
		if( gameDir[ i++ ] == '/')
		{
			a = &gameDir[ i ];
		}
	}

	modName->assign( a );
}

bool isLocalIp( const char* ip )
{
	if( strcmp( ip, "internalserver" ) == 0 || strcmp( ip, "localhost" ) == 0 || strcmp( ip, "loopback" ) == 0 )
	{
		return true;
	}
	else if( ip[0] == 10 || ip[0] == 127 || ( ip[0] == 172 && ip[1] > 15 && ip[1] < 32 ) || ( ip[0] == 192 && ip[1] == 168 ) )
	{ 
		return true; 
	}

	return false;
}

void retrieveServerIp( String* serverIp )
{
	if( serverIp->size() )
	{
		return;
	}

	const char* ip		= NULL;
	const char* port	= NULL;

	ip = CVAR_GET_STRING( "ip" );

	if( ip == NULL || *ip == EOS )
	{
		serverIp->assign( CVAR_GET_STRING( "net_address" ) );
	}
	else
	{
		serverIp->assign( ip );
		serverIp->append( ":" );

		atoi( port = CVAR_GET_STRING( "ip_hostport" ) ) || 
		atoi( port = CVAR_GET_STRING( "hostport" ) )    || 
		atoi( port = CVAR_GET_STRING( "port" ) );

		serverIp->append( port );	
	}
}

void retrieveClientIp( char ( &ip )[ 16 ], const char* source )
{
	const char*		ptr = source;
	uint32			i	= 0;
	char			ch;

	while( i < sizeof( ip ) && ( ch = *ptr ) && ch != ':' ) 
	{ 
		ip[ i ] = ch; i++, *ptr++;
	}

	ip[ i ] = EOS;
}

String normalizePath( String* file )
{
	int pos = 0;

	while( ( pos = file->find( '\\', pos ) ) != -1 ) 
	{
		file->at( pos, '/' );
	}

	return file->c_str();
}

String getMapPrefix( String mapName )
{
	uint32 position = mapName.find( '_' );

	if( position )
	{
		return mapName.substr( 0, position );
	}

	return NULL;
}

time_t getSysTime( void )
{
	time_t td = time( NULL );

	return td;
}

uint32 fileSize( const char* file )
{
	uint32	size = 0;
	char	fullpath[ 64 ];

	FILE* f = fopen( buildPathName( fullpath, charsmax( fullpath ), "%s", file ), "rb" );

	if( f )
	{
		fseek( f, 0, SEEK_END );
		size = ( int )ftell( f );
		fclose( f );
	}

	fullpath[ 0 ] = EOS;

	return size;
}

char* buildPathName( char *buffer, size_t maxlen, const char *fmt, ... )
{
	snprintf( buffer, maxlen, "%s%c", ModName.c_str(), PATH_SEP_CHAR );

	size_t len = strlen( buffer );
	char *ptr = buffer + strlen( buffer );

	va_list argptr;
	va_start( argptr, fmt );
	vsnprintf( ptr, maxlen - len, fmt, argptr );
	va_end( argptr );

	while( *ptr ) 
	{
		if (*ptr == ALT_SEP_CHAR )
		{
			*ptr = PATH_SEP_CHAR;
		}

		++ptr;
	}

	return buffer;
}

bool dirExists( const char *dir )
{
	char realDir[ 255 ];
	buildPathName( realDir, charsmax( realDir ), "%s", dir );

	#if defined WIN32 || defined _WIN32

		DWORD attr = GetFileAttributes( realDir );

		if( attr == INVALID_FILE_ATTRIBUTES )
			return false;

		if( attr & FILE_ATTRIBUTE_DIRECTORY )
			return true;

	#else

		struct stat s;

		if( stat( realDir, &s ) != 0 )
			return false;

		if( S_ISDIR( s.st_mode ) )
			return true;

	#endif

	return false;
}

bool fileExists( const char *file )
{
	char realFile[ 255 ];
	buildPathName( realFile, charsmax( realFile ), "%s", file );

	#if defined WIN32 || defined _WIN32

		DWORD attr = GetFileAttributes( realFile );

		if( attr == INVALID_FILE_ATTRIBUTES || attr == FILE_ATTRIBUTE_DIRECTORY )
			return false;

		return true;

	#else

		struct stat s;

		if( stat( realFile, &s ) != 0 || S_ISDIR( s.st_mode ) )
			return false;

		return true;

	#endif
}