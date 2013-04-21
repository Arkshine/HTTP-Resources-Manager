
#include "main.h"

CResourcesManager*	Config = NULL;

void OnMetaAttach( void )
{
	Config = new CResourcesManager;

	Config->handleCommand();
	Config->handleEngineConfig();
	Config->handleModName();
}

void OnServerActivatePost( edict_t* pEdictList, int edictCount, int clientMax )
{
	Config->currentServerSpawnCount++;

	RETURN_META( MRES_IGNORED );
}

int OnSpawn( edict_t* pEntity )
{
	if( !Config->initialized && Config->engineConfigReady )
	{
		Config->handleConfig();
		Config->initialized = true;
	}

	RETURN_META_VALUE( MRES_IGNORED, FALSE );
}

qboolean OnClientConnectPost( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	if( Config->initialized )
	{
		char	ip[ 16 ];
		uint32	player				= ENTINDEX( pEntity );
		time_t	timeSystem			= Config->getSysTime();
		time_t*	nextReconnectTime	= &Config->playerNextReconnectTime[ player ];
		String*	currentPlayerIp		= &Config->playerIp[ player ];

		// Retrieve server IP one time from there because
		// we need to let server.cfg be executed in case
		// a specific ip is provided and because I have not
		// found forward to use to put this call at this time.
		Config->retrieveServerIp();

		// Retrieve the current client's IP without the port.
		Config->retrieveClientIp( ip, pszAddress );

		// Sanity IP address check to make sure the index
		// is our expected player. If not, we consider it
		// as a new player and we reset variables to the default.
		if( currentPlayerIp->compare( ip ) )
		{
			*nextReconnectTime = 0;

			currentPlayerIp->clear();
			currentPlayerIp->assign( ip );
		}

		// We are allowed to reconnect the player.
		// We put a cooldown of 3 seconds minimum to avoid possible 
		// infinite loop since depending where it will download resources,
		// it can connect/disconnect several times.
		if( *nextReconnectTime < timeSystem )
		{
			*nextReconnectTime = timeSystem + 3;
			CLIENT_COMMAND( pEntity, "Connect %s %d\n", Config->serverIp.c_str(), RANDOM_LONG( 1, 9999 ) );
		}
	}

	RETURN_META_VALUE( MRES_IGNORED, TRUE );
}

void OnSteam_NotifyClientDisconnect( client_t* cl )
{
	GET_ORIG_FUNC( func );

	if( Config->initialized )
	{
		if( cl->resources )
		{
			// We are here because user was downloading resources from
			// game server and he has disconnected before be completed. 
			// So, at next connect we make sure it will connect from HTTP server.
			Config->playerNextReconnectTime[ cl->netchan.client_index ] = 0;
		}
	}

	if( func->Restore() )
	{
		Config->Steam_NotifyClientDisconnect( cl );
		func->Patch();
	}
}

void OnServerDeactivatePost( void )
{
	if( Config->initialized )
	{
		Config->customResourcesList.clear();
		Config->customUrlsList.clear();
		Config->moduleConfigStatus.clear();

		Config->initialized = false;
	}

	RETURN_META( MRES_IGNORED );
}

void OnMetaDetach( void )
{
	Config->clear();
}

void OnCommandResourceManager( void )
{
	const char*	command	= CMD_ARGV( 1 );

	if( !strcasecmp( command, "status" ) )  
	{		
		printf( Config->moduleStatus.c_str() );
	}
	else if( !strcasecmp( command, "config" ) )
	{
		printf( Config->moduleConfigStatus.c_str() );
	}
	else if( !strcasecmp( command, "version" ) )  
	{
		printf( "\n %s %s\n -\n", Plugin_info.name, Plugin_info.version );
		printf( " Support  : %s\n", Plugin_info.url );
		printf( " Author   : Arkshine\n" );
		printf( " Compiled : %s\n\n", __DATE__ ", " __TIME__ );
	}
	else
	{
		printf( "\n Usage: rm < command >\n" );
		printf( " Commands:\n");
		printf( "   version  - Display some informations about the module and where to get a support.\n");
		printf( "   status   - Display module status, showing if the memory stuffs have been well found.\n");
		printf( "   config   - Display the parsing of the config file. Useful to check to understand problems.\n\n");
	}
}

void OnSV_SendResources( sizebuf_t* buf )
{
	byte temprguc[ 32 ];

	memset( temprguc, 0, sizeof temprguc );

	Config->MSG_WriteByte( buf, SVC_RESOURCEREQUEST );
	Config->MSG_WriteLong( buf, Config->currentServerSpawnCount );
	Config->MSG_WriteLong( buf, 0 );

	String url = Config->getNextCustomUrl();

	if( url.c_str() && url.size() < MaxUrlLength )
	{
		Config->MSG_WriteByte( buf, SVC_RESOURCELOCATION);
		Config->MSG_WriteString( buf, url.c_str() );
	}

	Config->MSG_WriteByte( buf, SVC_RESOURCELIST );
	Config->MSG_StartBitWriting( buf );
	Config->MSG_WriteBits( Config->sv->consistencyDataCount + Config->customResourcesList.size(), 12 );

	resource_t* res = NULL;

	if( Config->sv->consistencyDataCount > 0 )
	{
		uint32 lastIndex = 0;
		uint32 genericItemsCount = 0;

		for( uint32 i = 0; i < Config->sv->consistencyDataCount; i++ )
		{
			res = &Config->sv->consistencyData[ i ];

			if( !genericItemsCount )
			{
				if( lastIndex && res->nIndex == 1 )
					genericItemsCount = lastIndex;
				else
					lastIndex = res->nIndex;
			}

			Config->MSG_WriteBits( res->type, 4 );
			Config->MSG_WriteBitString( res->szFileName );
			Config->MSG_WriteBits( res->nIndex, 12 );
			Config->MSG_WriteBits( res->nDownloadSize, 24 );
			Config->MSG_WriteBits( res->ucFlags & 3, 3 );

			if ( res->ucFlags & 4 )
				Config->MSG_WriteBitData( res->rgucMD5_hash, 16 );

			if ( memcmp( res->rguc_reserved, temprguc, sizeof temprguc ) )
			{
				Config->MSG_WriteBits( 1, 1 );
				Config->MSG_WriteBitData( res->rguc_reserved, 32 );
			}
			else
			{
				Config->MSG_WriteBits( 0, 1 );
			}
		}

		if( Config->customResourcesList.size() )
		{
			for( CVector< resource_s* >::iterator iter = Config->customResourcesList.begin(); iter != Config->customResourcesList.end(); ++iter )
			{
				res = ( *iter );

				Config->MSG_WriteBits( res->type, 4 );
				Config->MSG_WriteBitString( res->szFileName );
				Config->MSG_WriteBits( genericItemsCount + res->nIndex, 12 );
				Config->MSG_WriteBits( res->nDownloadSize, 24 );
				Config->MSG_WriteBits( res->ucFlags & 3, 3 );
				Config->MSG_WriteBits( 0, 1 );
			}
		}
	}

	Config->SV_SendConsistencyList();
	Config->MSG_EndBitWriting( buf );
}
