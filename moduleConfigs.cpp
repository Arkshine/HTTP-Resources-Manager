
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

cvar_t rm_enable_downloadfix	= { "rm_enable_downloadfix" , "0" };

void handleCvars( void )
{
	CVAR_REGISTER( &rm_enable_downloadfix );
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

		for( int type = MAP_DEFAULT; type <= MAP_NAME; type++ )
		{
			if( type == MAP_PREFIX && mapPrefix.c_str() == NULL )
			{
				continue;
			}

			switch( type )
			{ 
				case MAP_DEFAULT: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, ConfigDefaultResFileName );	break;
				case MAP_PREFIX : snprintf( file, charsmax( file ), "%s/prefix-%s.res", ConfigDirectory, mapPrefix.c_str() );	break;
				case MAP_NAME	: snprintf( file, charsmax( file ), "%s/%s.res", ConfigDirectory, mapName.c_str() );			break;
			}

			retrieveFileEntries( file, &resourcesList, &urlsList );
		}
		
		ModuleConfigStatus.append( "\n" );

		populateList( &CustomResourcesList, &resourcesList );	
		populateList( &CustomUrlsList, &urlsList );
	}
	else
	{
		ModuleConfigStatus.append( "\tMissing configuration directory.\n" );
	}

	ModuleConfigStatus.append( "\n Configuration initialization ended.\n\n" );

	// Don't show it by default.
	//printf( moduleConfigStatus.c_str() );
}

void retrieveFileEntries( const char* file, CVector< String >* resList, CVector< String >* urlList )
{
	ModuleConfigStatus.append( UTIL_VarArgs( "\n\tAttempting to parse \"%s\" file...\n\n", file ) );

	char path[ 255 ];
	buildPathName( path, charsmax( path ), "%s", file );

	FILE *fp = fopen( path, "rt" );
    
	if( !fp )
	{
		ModuleConfigStatus.append( "\t\t\tThe file doesn't exist or could not be opened.\n" );
		return;
	}

	ModuleConfigStatus.append( "\t\tSearching for resources and urls...\n\n" );

	char	lineRead[ 134 ], ch;
	String	line;
	int		length;

	const char downloadUrlIdent[] = "downloadurl";

	int downloadUrlsCount = 0;
	int resourcesFilesCount = 0;

	while( fgets( lineRead, charsmax( lineRead ), fp ) )
	{
		line = lineRead;
		line.trim();

		if( line.empty() || ( ch = line.at( 0 ) ) == ';' || ch == '#' || ch == '/' )
		{
			continue;
		}

		if( !strncasecmp( line.c_str(), downloadUrlIdent, charsmax( downloadUrlIdent ) ) )
		{
			line.erase( 0, charsmax( downloadUrlIdent ) );
			line.trim();

			normalizePath( &line );

			if( !isEntryDuplicated( line.c_str(), urlList ) )
			{
				urlList->push_back( line );
				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound url \"%s\"\n", line.c_str() ) );

				downloadUrlsCount++;
			}
			else
			{
				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound url \"%s\" > Duplicated, ignoring...\n", line.c_str() ) );
			}

			continue;
		}
		else
		{
			normalizePath( &line );

			size_t size = line.size();

			if( line.find( '/', size - 1 ) != line.npos )
			{
				line.erase( size - 1, 1 );
			}

			if( !dirExists( line.c_str() ) )
			{
				if( !fileExists( line.c_str() ) )
				{
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound directory \"%s\" > It doesn't seem to exist, ignoring...\n", line.c_str() ) );
				}
				else if( !isEntryDuplicated( line.c_str(), urlList ) )
				{
					urlList->push_back( line );
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound resource \"%s\"\n", line.c_str() ) );

					resourcesFilesCount++;
				}
				else
				{
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound resource \"%s\" > Duplicated, ignoring...\n", line.c_str() ) );
				}
			}
			else
			{
				#if defined WIN32

				WIN32_FIND_DATA fd;
				HANDLE hFile = FindFirstFile( buildPathName( path, charsmax( path ), "%s\\*", line.c_str() ), &fd );

				if( hFile == INVALID_HANDLE_VALUE )
				{
					ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound directory \"%s\" > Could not open it, ignoring...\n", line.c_str() ) );
					continue;
				}

				ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound directory \"%s\" > Searching inside...\n", line.c_str() ) );

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
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\" > Full path length > %d, ignoring... \n", fd.cFileName, MaxResLength ) );
								continue;
							}

							if( !isEntryDuplicated( path + ModName.size() + 1, resList ) )
							{
								resList->push_back( path + ModName.size() + 1 );
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\"\n", fd.cFileName ) );

								resourcesFilesCount++;
							}
							else
							{
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\" > Duplicated, ignoring...\n", fd.cFileName ) );
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
                    ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\tFound directory \"%s\" > Could not open it, ignoring...\n", line.c_str() ) );
					continue;
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
								ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\" > Full path length > %d, ignoring... \n", ep->d_name, MaxResLength ) );
                                continue;
							}

							if( !isEntryDuplicated( path + ModName.size() + 1, resList ) )
							{
								resList->push_back( path + ModName.size() + 1 );
                                ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\"\n", ep->d_name ) );

								resourcesFilesCount++;
							}
							else
							{
                                ModuleConfigStatus.append( UTIL_VarArgs( "\t\t\t\tFound resource \"%s\" > Duplicated, ignoring...\n", ep->d_name ) );
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

	if( !resourcesFilesCount && !downloadUrlsCount )
	{
		ModuleConfigStatus.append( "\t\t\tNo entries found.\n" );
	}
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