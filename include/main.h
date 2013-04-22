
#ifndef MAIN_H
#define MAIN_H

#include "amxxmodule.h"

#include <stdio.h>
#include <string.h>

#include "configs.h"
#include "chooker.h"

#if defined __linux__
	#include <sys/types.h>
	#include <dirent.h>
#endif

typedef void	( *FuncSV_SendResources )				( sizebuf_t* );
typedef void	( *FuncSV_SendConsistencyList )			( void );
typedef void	( *FuncMSG_WriteBits )					( uint32, int );
typedef void	( *FuncMSG_StartBitWriting )			( sizebuf_t* );
typedef void	( *FuncMSG_EndBitWriting )				( sizebuf_t* );
typedef void	( *FuncSteam_NotifyClientDisconnect )	( client_t* );

void	OnSV_SendResources				( sizebuf_t* buf );
void	OnSteam_NotifyClientDisconnect	( client_t* cl );
void	OnCommandResourceManager		( void );

class CResourcesManager
{
	private:

		void SZ_Clear( sizebuf_t *buf ) 
		{
			buf->cursize = 0;
			buf->overflow = 0;
		}

		void* SZ_GetSpace( sizebuf_t* buf, unsigned int length ) 
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

	public:

		CHooker* hooker;

		char dataDefaultResFile[ sizeof ConfigDirectory + sizeof ConfigDefaultResFileName + MaxModLength ];

		String moduleStatus;
		String moduleConfigStatus;
		String modName;
		String serverIp;

		cvar_t*	sv_downloadurl;
		cvar_t* ip;
		cvar_t* ip_hostport;
		cvar_t* hostport;
		cvar_t* port;
		cvar_t* developer;

		bool missingConfigs;
		bool engineConfigReady;
		bool initialized;
		bool sendResourcesHookCreated;
		bool notifyClientDisconnectHookCreated;

		FuncSV_SendResources				SV_SendResources;
		FuncSV_SendConsistencyList			SV_SendConsistencyList;
		FuncMSG_WriteBits					MSG_WriteBits;
		FuncMSG_StartBitWriting				MSG_StartBitWriting;
		FuncMSG_EndBitWriting				MSG_EndBitWriting;
		FuncSteam_NotifyClientDisconnect	Steam_NotifyClientDisconnect;

		#if defined _WIN32
			void* Host_IsServerActive;
		#endif 

		server_s* sv;

		CVector< resource_s* >	customResourcesList;
		CVector< String >		customUrlsList;

		uint32 currentServerSpawnCount;
		uint32 currentUrlIndex;

		String playerIp[ MaxClients + 1 ];
		time_t playerNextReconnectTime[ MaxClients + 1 ];

		enum
		{
			MAP_PREFIX = 1,
			MAP_NAME,
			MAP_DEFAULT
		};

		CResourcesManager() 
		{
			hooker = new CHooker;

			sv_downloadurl	= NULL;
			ip				= NULL;
			ip_hostport		= NULL;
			hostport		= NULL;
			port			= NULL;
			developer		= NULL;

			missingConfigs						= false;
			engineConfigReady					= false;
			initialized							= false;
			sendResourcesHookCreated			= false;
			notifyClientDisconnectHookCreated	= false;

			SV_SendResources				= NULL;
			SV_SendConsistencyList			= NULL;
			MSG_WriteBits					= NULL;
			MSG_StartBitWriting				= NULL;
			MSG_EndBitWriting				= NULL;
			Steam_NotifyClientDisconnect	= NULL;
			sv								= NULL;

			#if defined _WIN32
				Host_IsServerActive = NULL;
			#endif 

			currentServerSpawnCount = 0;
			currentUrlIndex			= 0;

			for( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				playerNextReconnectTime[ i ] = 0;
			}
		};

		~CResourcesManager() 
		{
			clear();
		}

		void clear()
		{
			delete hooker;

			moduleStatus.clear();
			moduleConfigStatus.clear();
			modName.clear();
			serverIp.clear();

			customResourcesList.clear();
			customUrlsList.clear();

			for( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				playerIp[ i ].clear();
			}
		}

		void handleCommand( void )
		{
			REG_SVR_COMMAND( "rm", OnCommandResourceManager );
		}

		void handleEngineConfig( void )
		{
			sv_downloadurl	= CVAR_GET_POINTER( "sv_downloadurl" );
			developer		= CVAR_GET_POINTER( "developer" );

			moduleStatus.assign( UTIL_VarArgs( "\n %s v%s - by %s.\n", Plugin_info.name, Plugin_info.version, Plugin_info.author ) );
			moduleStatus.append( " Memory initialization started.\n" );

			engineConfigReady = findFunctions()				&& 
								findServerStructAddress()	&& 
								createSendResourcesHook();
	
			moduleStatus.append
			( 
				UTIL_VarArgs
				(
					"\n\t[%s] Finding \"SV_SendResources\" function			\
					 \n\t[%s] Finding \"SV_SendConsistencyList\" function	\
					 \n\t[%s] Finding \"MSG_WriteBits\" function			\
					 \n\t[%s] Finding \"MSG_StartBitWriting\" function		\
					 \n\t[%s] Finding \"MSG_EndBitWriting\" function		\
					 \n\t[%s] Finding \"Steam_NotifyClientDisconnect\" function",
					SV_SendResources			? "FOUND" : "NOT FOUND",
					SV_SendConsistencyList		? "FOUND" : "NOT FOUND",
					MSG_WriteBits				? "FOUND" : "NOT FOUND",
					MSG_StartBitWriting			? "FOUND" : "NOT FOUND",
					MSG_EndBitWriting			? "FOUND" : "NOT FOUND",
					Steam_NotifyClientDisconnect? "FOUND" : "NOT FOUND"
				) 
			);

		#if defined _WIN32
			moduleStatus.append( UTIL_VarArgs( "\n\t[%s] Finding \"Host_IsServerActive\" function", Host_IsServerActive ? "FOUND" : "NOT FOUND" ) );
		#endif
			moduleStatus.append( UTIL_VarArgs( "\n\t[%s] Finding \"sv\" global variable", sv != NULL ? "FOUND" : "NOT FOUND" ) );
			moduleStatus.append( UTIL_VarArgs( "\n\n\t[%s] Creating \"SV_SendResourcesHook\" hook", sendResourcesHookCreated ? "SUCCESS" : "FAILED" ) );
			moduleStatus.append( UTIL_VarArgs( "\n\t[%s] Creating \"Steam_NotifyClientDisconnect\" hook\n\n", notifyClientDisconnectHookCreated ? "SUCCESS" : "FAILED" ) );
			moduleStatus.append( " Memory initialization ended.\n\n" );

			printf( moduleStatus.c_str() );
		}

		void handleConfig( void )
		{
			moduleConfigStatus.append( " Configuration initialization started.\n\n" );

			if( !( missingConfigs = !dirExists( ConfigDirectory ) ) )
			{
				String		mapName;
				String		mapPrefix;
				char		file[ 256 ];

				CVector< String >	resourcesList;
				CVector< String >	urlsList;

				mapName.assign( STRING( gpGlobals->mapname ) );
				mapPrefix = getMapPrefix( mapName );

				moduleConfigStatus.append( UTIL_VarArgs( "\tMap name : %s\n\tMap prefix : %s\n", mapName.c_str(), mapPrefix.c_str() ) );

				for( uint32 type = mapPrefix.c_str() ? MAP_PREFIX : MAP_NAME; type <= MAP_DEFAULT; type++ )
				{
					switch( type )
					{
						case MAP_PREFIX : snprintf( file, charsmax( file ), "%s/prefix-%s.res", ConfigDirectory, mapPrefix.c_str() );	break;
						case MAP_NAME	: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, mapName.c_str() );			break;
						case MAP_DEFAULT: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, ConfigDefaultResFileName );	break;
					}

					if( fileExists( file ) && retrieveFileEntries( file, &resourcesList, &urlsList ) )
					{
						break;
					}
				}

				moduleConfigStatus.append( "\n" );

				populateList( &customResourcesList, &resourcesList );	
				populateList( &customUrlsList, &urlsList );
			}
			else
			{
				moduleConfigStatus.append( "\tMissing configuration directory.\n" );
			}

			moduleConfigStatus.append( "\n Configuration initialization started.\n\n" );

			// Don't show it by default.
			//printf( moduleConfigStatus.c_str() );
		}

		void handleModName( void )
		{
			char gameDir[ 512 ];
			GET_GAME_DIR( gameDir );

			char *a = gameDir;

			int i = 0;

			while( gameDir[ i ] )
			{
				if( gameDir[ i++ ] == '/')
				{
					a = &gameDir[ i ];
				}
			}

			modName.assign( a );
		}


		bool findFunctions( void )
		{
			void* engineAddress = hooker->memFunc->GetLibraryFromAddress( ( void* )gpGlobals );

			#if defined __linux__
				BOOL useSymbol = TRUE;
			#else
				BOOL useSymbol = FALSE;
			#endif

			hooker->memFunc->SearchByLibrary( &SV_SendResources		, FUNC_SV_SENDRESOURCES_DS		, engineAddress, useSymbol );
			hooker->memFunc->SearchByLibrary( &SV_SendConsistencyList, FUNC_SV_SENDCONSISTENCYLIST_DS, engineAddress, useSymbol );
			hooker->memFunc->SearchByLibrary( &MSG_WriteBits		, FUNC_MSG_WRITEBITS_DS			, engineAddress, useSymbol );
			hooker->memFunc->SearchByLibrary( &MSG_StartBitWriting	, FUNC_MSG_STARTBITWRITING_DS	, engineAddress, useSymbol );
			hooker->memFunc->SearchByLibrary( &MSG_EndBitWriting	, FUNC_MSG_ENDBITWRITING_DS		, engineAddress, useSymbol );
			hooker->memFunc->SearchByLibrary( &Steam_NotifyClientDisconnect, FUNC_STEAM_NOTIFYCLIENTDISCONNECT_DS, engineAddress, useSymbol );

			#if defined __linux__

				return	SV_SendResources				&& 
						SV_SendConsistencyList			&& 
						MSG_WriteBits					&& 
						MSG_StartBitWriting				&& 
						MSG_EndBitWriting				&& 
						Steam_NotifyClientDisconnect;

			#else

				hooker->memFunc->SearchByLibrary( &Host_IsServerActive, FUNC_HOST_ISSERVERACTIVE_DS, engineAddress, FALSE );

				return	SV_SendResources				&& 
						SV_SendConsistencyList			&& 
						MSG_WriteBits					&& 
						MSG_StartBitWriting				&& 
						MSG_EndBitWriting				&& 
						Host_IsServerActive				&& 
						Steam_NotifyClientDisconnect;	 
				
			#endif
		}

		bool findServerStructAddress( void )
		{
			#if defined __linux__
				hooker->memFunc->SearchByLibrary( &sv, FUNC_SV_DS, ( void* )gpGlobals, TRUE );
			#else
				sv = ( server_s* )*( unsigned long* )( ( unsigned char* )Host_IsServerActive + 1 );
			#endif

			return sv != NULL;
		}

		bool createSendResourcesHook( void )
		{
			if( SV_SendResources )
			{
				sendResourcesHookCreated = hooker->AddHook( ( void* )SV_SendResources, ( void* )OnSV_SendResources ) ? true : false;
			}

			if( Steam_NotifyClientDisconnect )
			{
				notifyClientDisconnectHookCreated = hooker->AddHook( ( void* )Steam_NotifyClientDisconnect, ( void* )OnSteam_NotifyClientDisconnect ) ? true : false;
			}

			return sendResourcesHookCreated && notifyClientDisconnectHookCreated;
		}


		String getNextCustomUrl( void )
		{
			if( customUrlsList.size() )
			{
				uint32 count = 0;

				for( CVector< String >::iterator iter = customUrlsList.begin(); iter != customUrlsList.end(); ++iter )
				{
					if( count++ == currentUrlIndex )
					{
						if( ++currentUrlIndex >= customUrlsList.size() )
						{
							currentUrlIndex = 0;
						}

						return ( *iter );
					}
				}
			}

			return sv_downloadurl->string;
		}


		bool retrieveFileEntries( const char* file, CVector< String >* resList, CVector< String >* urlList )
		{
			moduleConfigStatus.append( UTIL_VarArgs( "\n\tAttempting to parse \"%s\" file...\n", file ) );

			char path[ 255 ];
			buildPathName( path, charsmax( path ), "%s", file );

			FILE *fp = fopen( path, "rt" );

			if( !fp )
			{
				moduleConfigStatus.append( "\t\t(!) Could not open file, even though it exists.\n\n" );
				return false;
			}

			bool searchForResources = !resList->size();
			bool searchForUrls		= !urlList->size();

			moduleConfigStatus.append( UTIL_VarArgs( "\t\tSearching for %s%s%s...\n", 
				searchForResources ? "resources" : "", 
				searchForResources && searchForUrls ? " and " : "",
				searchForUrls ? "urls" : "" ) );

			char	lineRead[ 134 ], ch;
			String	line;
			int		length;

			const char downloadUrlIdent[] = "downloadurl";

			while( fgets( lineRead, charsmax( lineRead ), fp ) )
			{
				line = lineRead;
				line.trim();

				if( line.empty() || ( ch = line.at( 0 ) ) == ';' || ch == '#' || ch == '/' )
				{
					continue;
				}

				if( searchForUrls && !strncasecmp( line.c_str(), downloadUrlIdent, charsmax( downloadUrlIdent ) ) )
				{
					line.erase( 0, charsmax( downloadUrlIdent ) );
					line.trim();

					urlList->push_back( normalizePath( &line ) );

					moduleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound \"%s\"\n", line.c_str() ) );

					continue;
				}
				else if( searchForResources )
				{
					normalizePath( &line );

					size_t size = line.size();

					if( line.find( '/', size - 1 ) != line.npos )
					{
						line.erase( size - 1, 1 );
					}

					if( !dirExists( line.c_str() ) )
					{
						resList->push_back( normalizePath( &line ) );

						moduleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound \"%s\"\n", line.c_str() ) );
					}
					else
					{
						moduleConfigStatus.append( UTIL_VarArgs( "\t\t\tSearching inside \"%s\" directory...\n", line.c_str() ) );

						#if defined WIN32

						WIN32_FIND_DATA fd;
						HANDLE hFile = FindFirstFile( buildPathName( path, charsmax( path ), "%s\\*", line.c_str() ), &fd );

						if( hFile == INVALID_HANDLE_VALUE )
						{
							moduleConfigStatus.append( "\t\t\t\t(!)Could not open directory. Skipping...\n" );
							continue;
						}

						path[ length = strlen( path ) - 1 ] = EOS;

						do
						{
							if( fd.cFileName[ 0 ] != '.' && strcmp( &fd.cFileName[ strlen( fd.cFileName ) - 5 ], ".ztmp" ) && strcmp( &fd.cFileName[ strlen( fd.cFileName ) - 3 ], ".db" ) )
							{
								snprintf( path + length, MaxResLength - length, "%s", fd.cFileName );

								if( dirExists( path + modName.size() + 1 ) )
								{
									continue;
								}
								else if( fileExists( path + modName.size() + 1 ) )
								{
									if( strlen( path + modName.size() + 1 ) > MaxResLength )
									{
										moduleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tSkipping \"%s\" file (full path length > %d)... \n", fd.cFileName, MaxResLength ) );
										continue;
									}

									resList->push_back( path + modName.size() + 1 );

									moduleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound \"%s\"\n", fd.cFileName ) );
								}
							}
						}
						while( FindNextFile( hFile, &fd ) );

						FindClose( hFile );

						#elif defined __linux__

						struct dirent *ep;
						DIR *dp;

						if( ( dp = opendir ( buildPathName( path, charsmax( path ), "%s", line.c_str() ) ) ) == NULL )
						{
							moduleConfigStatus.append( "\t\t\t\t(!)Could not open directory. Skipping...\n" );
							return false;
						}

						path[ length = strlen( path ) ] = EOS;

						while( ( ep = readdir( dp ) ) != NULL )
						{
							if( ep->d_name[ 0 ] != '.' && strcmp( &ep->d_name[ strlen( ep->d_name ) - 5 ], ".ztmp" ) && strcmp( &ep->d_name[ strlen( ep->d_name ) - 3 ], ".db" ) ) 
							{
								snprintf( path + length, MaxResLength - length, "/%s", ep->d_name );
						
								if( dirExists( path + modName.size() + 1 ) )
								{
									continue;
								}
								else if( fileExists( path + modName.size() + 1 ) )
								{
									if( strlen( path + modName.size() + 1 ) > MaxResLength )
									{
										moduleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tSkipping \"%s\" file (full path length > %d)... \n", ep->d_name, MaxResLength ) );
										continue;
									}

									resList->push_back( path + modName.size() + 1 );

									moduleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound \"%s\"\n", ep->d_name ) );
								}
							}
						}

						closedir( dp );

						#endif
					}
				}

				path[ 0 ] = EOS;
			}

			fclose( fp );

			bool result = resList->size() && urlList->size();

			if( !result )
			{
				moduleConfigStatus.append( "\t\t\tNo entries found.\n" );
			}

			return result;
		}


		void populateList( CVector< resource_t* >* list, CVector< String >* entries )
		{
			if( entries->size() )
			{
				resource_t* res;
				String		file;
				int			count = 0;

				for( CVector< String >::iterator iter = entries->begin(); iter != entries->end(); ++iter )
				{
					file = ( *iter );

					res = new resource_t;

					strcpy( res->szFileName, normalizePath( &file ).c_str() );

					res->szFileName[ sizeof( res->szFileName ) - 1 ] = EOS;

					res->type			= t_generic;
					res->nIndex			= ++count;
					res->nDownloadSize	= fileSize( file.c_str() );
					res->ucFlags		= 0;

					list->push_back( res );
				}

				moduleConfigStatus.append( UTIL_VarArgs( "\tFound \"%d\" resources.\n", entries->size() ) );
			}

			entries->clear();
		}

		void populateList( CVector< String >* list, CVector< String >* entries )
		{
			if( entries->size() )
			{
				char url[ MaxUrlLength ];

				for( CVector< String >::iterator iter = entries->begin(); iter != entries->end(); ++iter )
				{
					strncpy( url, normalizePath( &( *iter ) ).c_str(), MaxUrlLength - 1 );

					list->push_back( url );
				}

				moduleConfigStatus.append( UTIL_VarArgs( "\tFound \"%d\" urls.\n", entries->size() ) );
			}
			
			entries->clear();
		}


		void retrieveServerIp( void )
		{
			if( serverIp.size() )
			{
				return;
			}

			const char* ip		= NULL;
			const char* port	= NULL;

			serverIp.clear();

			if( !( ip = CVAR_GET_STRING( "ip" ) ) )
			{
				serverIp.assign( CVAR_GET_STRING( "net_address" ) );
			}
			else
			{
				serverIp.assign( ip );
				serverIp.append( ":" );

				atoi( port = CVAR_GET_STRING( "ip_hostport" ) ) || 
				atoi( port = CVAR_GET_STRING( "hostport" ) )    || 
				atoi( port = CVAR_GET_STRING( "port" ) );

				serverIp.append( port );	
			}
		}

		void retrieveClientIp( char ( &ip )[ 16 ], const char* source )
		{
			const char*		ptr = source;
			unsigned int	i	= 0;
			char			ch;

			while( i < sizeof( ip ) && ( ch = *ptr ) && ch != ':' ) 
			{ 
				ip[ i ] = ch; i++, *ptr++;
			}

			ip[ i ] = EOS;
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


		int fileSize( const char* file )
		{
			int		size = 0;
			char	fullpath[ 64 ];

			FILE* f = fopen( buildPathName( fullpath, charsmax( fullpath ), "%s", file ), "rb" );

			if( f )
			{
				fseek( f, 0, SEEK_END );
				size = ( int )ftell( f );
			}

			fullpath[ 0 ] = EOS;

			return size;
		}

		char* buildPathName( char *buffer, size_t maxlen, char *fmt, ... )
		{
			snprintf( buffer, maxlen, "%s%c", modName.c_str(), PATH_SEP_CHAR );

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

};


#endif // MAIN_H