
#ifndef MODULE_UTILS_H
#define MODULE_UTILS_H

#include <time.h>
#include "engineConfigs.h"

#if defined __linux__
	#include <sys/types.h>
	#include <dirent.h>
#endif

#define charsmax(a) ( sizeof( a ) - 1 )
#define EOS			'\0'

#ifndef __linux__
	#define PATH_SEP_CHAR		'\\'
	#define ALT_SEP_CHAR		'/'
#else
	#define PATH_SEP_CHAR		'/'
	#define ALT_SEP_CHAR		'\\'
#endif

void	retrieveModName	( String* modName );
void	retrieveServerIp( String* serverIp );
void	retrieveClientIp( char ( &ip )[ 16 ], const char* source );
bool	isLocalIp		( const char* ip );

String	getMapPrefix	( String mapName );
time_t	getSysTime		( void );

String	normalizePath	( String* file );
char*	buildPathName	( char *buffer, size_t maxlen, const char *fmt, ... );

bool	dirExists		( const char *dir );
void    makeDir         ( const char *realpath );
bool	fileExists		( const char *file );
uint32	fileSize		( const char* file );

#endif // MODULE_UTILS_H
