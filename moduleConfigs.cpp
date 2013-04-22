
#include "moduleConfigs.h"
#include "moduleUtils.h"
#include "engineCvars.h"

String ModName;
String ModuleStatus;
String ModuleConfigStatus;

bool MissingConfigs						= false;
bool EngineConfigReady					= false;
bool Initialized						= false;
bool SendResourcesHookCreated			= false;
bool NotifyClientDisconnectHookCreated	= false;

CVector< resource_s* >	CustomResourcesList;
CVector< String >		CustomUrlsList;

char dataDefaultResFile[ sizeof ConfigDirectory + sizeof ConfigDefaultResFileName + MaxModLength ];

uint32 CurrentUrlIndex = 0;

String PlayerCurrentIp			[ MaxClients + 1 ];
time_t PlayerNextReconnectTime	[ MaxClients + 1 ];

cvar_t rm_default_fallback = { "rm_default_fallback", "0" };


void handleCvars( void )
{
	CVAR_REGISTER( &rm_default_fallback );
}

void handleConfig( void )
{
	ModuleConfigStatus.append( " Configuration initialization started.\n\n" );

	if( !( MissingConfigs = !dirExists( ConfigDirectory ) ) )
	{
		String		mapName;
		String		mapPrefix;
		char		file[ 256 ];

		CVector< String >	resourcesList;
		CVector< String >	urlsList;

		mapName.assign( STRING( gpGlobals->mapname ) );
		mapPrefix = getMapPrefix( mapName );

		ModuleConfigStatus.append( UTIL_VarArgs( "\tMap name : %s\n\tMap prefix : %s\n", mapName.c_str(), mapPrefix.c_str() == NULL ? "(no prefix)" : mapPrefix.c_str() ) );

		for( uint32 type = mapPrefix.c_str() ? MAP_PREFIX : MAP_NAME; type <= MAP_DEFAULT; type++ )
		{
			switch( type )
			{
				case MAP_PREFIX : snprintf( file, charsmax( file ), "%s/prefix-%s.res", ConfigDirectory, mapPrefix.c_str() );	break;
				case MAP_NAME	: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, mapName.c_str() );			break;
				case MAP_DEFAULT: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, ConfigDefaultResFileName );	break;
			}

			if( rm_default_fallback.value > 0 )
			{
				if( fileExists( file ) && retrieveFileEntries( file, &resourcesList, &urlsList ) )
				{
					break;
				}
			}
			else
			{
				if( fileExists( file ) )
				{
					retrieveFileEntries( file, &resourcesList, &urlsList, type == MAP_PREFIX || type == MAP_DEFAULT );
					continue;
				}
			}
		}
		
		ModuleConfigStatus.append( "\n" );

		populateList( &CustomResourcesList, &resourcesList );	
		populateList( &CustomUrlsList, &urlsList );
	}
	else
	{
		ModuleConfigStatus.append( "\tMissing configuration directory.\n" );
	}

	ModuleConfigStatus.append( "\n Configuration initialization started.\n\n" );

	// Don't show it by default.
	//printf( moduleConfigStatus.c_str() );
}

bool retrieveFileEntries( const char* file, CVector< String >* resList, CVector< String >* urlList, bool required )
{
	ModuleConfigStatus.append( UTIL_VarArgs( "\n\tAttempting to parse \"%s\" file...\n", file ) );

	char path[ 255 ];
	buildPathName( path, charsmax( path ), "%s", file );

	FILE *fp = fopen( path, "rt" );

	if( !fp )
	{
		ModuleConfigStatus.append( "\t\t(!) Could not open file, even though it exists.\n\n" );
		return false;
	}

	bool searchForResources	= true;
	bool searchForUrls		= true;

	if( !required )
	{
		searchForResources	= !resList->size();
		searchForUrls		= !urlList->size();
	}

	ModuleConfigStatus.append( UTIL_VarArgs( "\t\tSearching for %s%s%s...\n", 
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

			normalizePath( &line );

			if( !isEntryDuplicated( line.c_str(), urlList ) )
			{
				urlList->push_back( line );
				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound \"%s\"\n", line.c_str() ) );
			}
			else
			{
				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound duplicated \"%s\", ignoring...\n", line.c_str() ) );
			}

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
				if( !isEntryDuplicated( line.c_str(), urlList ) )
				{
					urlList->push_back( line );
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound \"%s\"\n", line.c_str() ) );
				}
				else
				{
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound duplicated \"%s\", ignoring...\n", line.c_str() ) );
				}
			}
			else
			{
				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tSearching inside \"%s\" directory...\n", line.c_str() ) );

				#if defined WIN32

				WIN32_FIND_DATA fd;
				HANDLE hFile = FindFirstFile( buildPathName( path, charsmax( path ), "%s\\*", line.c_str() ), &fd );

				if( hFile == INVALID_HANDLE_VALUE )
				{
					ModuleConfigStatus.append( "\t\t\t\t(!)Could not open directory. Skipping...\n" );
					continue;
				}

				path[ length = strlen( path ) - 1 ] = EOS;

				do
				{
					if( fd.cFileName[ 0 ] != '.' && strcmp( &fd.cFileName[ strlen( fd.cFileName ) - 5 ], ".ztmp" ) && strcmp( &fd.cFileName[ strlen( fd.cFileName ) - 3 ], ".db" ) )
					{
						snprintf( path + length, MaxResLength - length, "%s", fd.cFileName );

						if( dirExists( path + ModName.size() + 1 ) )
						{
							continue;
						}
						else if( fileExists( path + ModName.size() + 1 ) )
						{
							if( strlen( path + ModName.size() + 1 ) > MaxResLength )
							{
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tSkipping \"%s\" file (full path length > %d)... \n", fd.cFileName, MaxResLength ) );
								continue;
							}

							if( !isEntryDuplicated( path + ModName.size() + 1, resList ) )
							{
								resList->push_back( path + ModName.size() + 1 );
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound \"%s\"\n", fd.cFileName ) );
							}
							else
							{
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound duplicated \"%s\", ignoring...\n", fd.cFileName ) );
							}
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
					ModuleConfigStatus.append( "\t\t\t\t(!)Could not open directory. Skipping...\n" );
					return false;
				}

				path[ length = strlen( path ) ] = EOS;

				while( ( ep = readdir( dp ) ) != NULL )
				{
					if( ep->d_name[ 0 ] != '.' && strcmp( &ep->d_name[ strlen( ep->d_name ) - 5 ], ".ztmp" ) && strcmp( &ep->d_name[ strlen( ep->d_name ) - 3 ], ".db" ) ) 
					{
						snprintf( path + length, MaxResLength - length, "/%s", ep->d_name );
						
						if( dirExists( path + ModName.size() + 1 ) )
						{
							continue;
						}
						else if( fileExists( path + ModName.size() + 1 ) )
						{
							if( strlen( path + ModName.size() + 1 ) > MaxResLength )
							{
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tSkipping \"%s\" file (full path length > %d)... \n", ep->d_name, MaxResLength ) );
								continue;
							}

							if( isEntryDuplicated( path + ModName.size() + 1, resList ) )
							{
								resList->push_back( path + ModName.size() + 1 );
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound \"%s\"\n", ep->d_name ) );
							}
							else
							{
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound duplicated \"%s\", ignoring...\n", ep->d_name ) );
							}
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
		ModuleConfigStatus.append( "\t\t\tNo entries found.\n" );
	}

	return result;
}

bool isEntryDuplicated( const char* entry, CVector< String >* entriesList )
{
	if( entriesList->size() )
	{
		for( CVector< String >::iterator iter = entriesList->begin(); iter != entriesList->end(); ++iter )
		{
			if( !( *iter ).compare( entry ) )
			{
				return true;
			}
		}
	}

	return false;
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

		ModuleConfigStatus.append( UTIL_VarArgs( "\tFound \"%d\" resources.\n", entries->size() ) );
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

		ModuleConfigStatus.append( UTIL_VarArgs( "\tFound \"%d\" urls.\n", entries->size() ) );
	}
			
	entries->clear();
}

String getNextCustomUrl( void )
{
	if( CustomUrlsList.size() )
	{
		uint32 count = 0;

		for( CVector< String >::iterator iter = CustomUrlsList.begin(); iter != CustomUrlsList.end(); ++iter )
		{
			if( count++ == CurrentUrlIndex )
			{
				if( ++CurrentUrlIndex >= CustomUrlsList.size() )
				{
					CurrentUrlIndex = 0;
				}

				return ( *iter );
			}
		}
	}

	return sv_downloadurl->string;
}