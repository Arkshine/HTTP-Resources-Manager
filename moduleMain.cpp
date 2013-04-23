
#include "moduleMain.h"

void OnMetaAttach( void )
{
	REG_SVR_COMMAND( "rm", OnCommandResourceManager );

	handleEngineConfig();
	handleCvars();
	
	retrieveModName( &ModName );
	retrieveServerIp( &ServerLocalIp );
}

void OnCommandResourceManager( void )
{
	const char*	command	= CMD_ARGV( 1 );

	if( !strcasecmp( command, "status" ) )  
	{	
		printf( ModuleStatus.c_str() );
		printf( " Status : " );

		if( !EngineConfigReady )
			printf( "Not running - Problem detected during memory initialization.\n\n" );

		else if( sv_allowdownload->value <= 0 )
			printf( "Not running - You must allow download by setting sv_allowdownload cvar to 1.\n\n" );

		else
			printf( "Running.\n\n" );
	}
	else if( !strcasecmp( command, "config" ) )
	{
		printf( ModuleConfigStatus.c_str() );
	}
	else if( !strcasecmp( command, "version" ) )  
	{
		printf( "\n %s %s\n -\n", MODULE_NAME, MODULE_VERSION );
		printf( " Support  : %s\n", MODULE_URL );
		printf( " Author   : Arkshine\n" );
		printf( " Compiled : %s, %s\n\n", __DATE__, __TIME__ );
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

void OnServerActivatePost( edict_t* pEdictList, int edictCount, int clientMax )
{
	CurrentServerSpawnCount++;

	RETURN_META( MRES_IGNORED );
}

int OnSpawn( edict_t* pEntity )
{
	if( !Initialized && EngineConfigReady )
	{
		handleConfig();

		Initialized = true;
	}

	RETURN_META_VALUE( MRES_IGNORED, FALSE );
}

qboolean OnClientConnectPost( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	if( Initialized && sv_allowdownload->value > 0 )
	{
		char	ip[ 16 ];
		uint32	player				= ENTINDEX( pEntity );
		time_t	timeSystem			= getSysTime();
		time_t*	nextReconnectTime	= &PlayerNextReconnectTime[ player ];
		String*	currentPlayerIp		= &PlayerCurrentIp[ player ];

		// Retrieve server IP one time from there because
		// we need to let server.cfg be executed in case
		// a specific ip is provided and because I have not
		// found forward to use to put this call at this time.
		retrieveServerIp( &ServerInternetIp );

		// Retrieve the current client's IP without the port.
		retrieveClientIp( ip, pszAddress );

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
		// We put a cool down of 3 seconds minimum to avoid possible 
		// infinite loop since depending where it will download resources,
		// it can connect/disconnect several times.
		if( *nextReconnectTime < timeSystem )
		{
			*nextReconnectTime = timeSystem + 3;
			CLIENT_COMMAND( pEntity, "Connect %s %d\n", isLocalIp( ip ) ? ServerLocalIp.c_str() : ServerInternetIp.c_str(), RANDOM_LONG( 1, 9999 ) );
		}
	}

	RETURN_META_VALUE( MRES_IGNORED, TRUE );
}

void OnSteam_NotifyClientDisconnect( client_t* cl )
{
	GET_ORIG_FUNC( func );

	if( Initialized && sv_allowdownload->value > 0 )
	{
		if( cl->resources )
		{
			// We are here because user was downloading resources from
			// game server and he has disconnected before be completed. 
			// So, at next connect we make sure it will connect from HTTP server.
			PlayerNextReconnectTime[ cl->netchan.client_index ] = 0;
		}
	}

	if( func->Restore() )
	{
		Steam_NotifyClientDisconnect( cl );
		func->Patch();
	}
}

void OnServerDeactivatePost( void )
{
	if( Initialized )
	{
		CustomResourcesList.clear();
		CustomUrlsList.clear();

		ModuleConfigStatus.clear();

		Initialized = false;
	}

	RETURN_META( MRES_IGNORED );
}

void OnMetaDetach( void )
{
	ModuleStatus.clear();
	ModuleConfigStatus.clear();

	ModName.clear();

	ServerLocalIp.clear();
	ServerInternetIp.clear();

	CustomResourcesList.clear();
	CustomUrlsList.clear();

	for( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		PlayerCurrentIp[ i ].clear();
	}
}

void OnSV_SendResources( sizebuf_t* buf )
{
	printf( "OnSV_SendResources\n" );

	byte temprguc[ 32 ];

	memset( temprguc, 0, sizeof temprguc );

	MSG_WriteByte( buf, SVC_RESOURCEREQUEST );
	MSG_WriteLong( buf, CurrentServerSpawnCount );
	MSG_WriteLong( buf, 0 );

	String url = getNextCustomUrl();

	if( url.c_str() && url.size() < MaxUrlLength )
	{
		MSG_WriteByte( buf, SVC_RESOURCELOCATION);
		MSG_WriteString( buf, url.c_str() );
	}

	MSG_WriteByte( buf, SVC_RESOURCELIST );
	MSG_StartBitWriting( buf );
	MSG_WriteBits( sv->consistencyDataCount + CustomResourcesList.size(), 12 );

	resource_t* res = NULL;

	if( sv->consistencyDataCount > 0 )
	{
		uint32 lastIndex = 0;
		uint32 genericItemsCount = 0;

		for( uint32 i = 0; i < sv->consistencyDataCount; i++ )
		{
			res = &sv->consistencyData[ i ];

			if( !genericItemsCount )
			{
				if( lastIndex && res->nIndex == 1 )
					genericItemsCount = lastIndex;
				else
					lastIndex = res->nIndex;
			}

			MSG_WriteBits( res->type, 4 );
			MSG_WriteBitString( res->szFileName );
			MSG_WriteBits( res->nIndex, 12 );
			MSG_WriteBits( res->nDownloadSize, 24 );
			MSG_WriteBits( res->ucFlags & 3, 3 );

			if( res->ucFlags & 4 )
				MSG_WriteBitData( res->rgucMD5_hash, 16 );

			if( memcmp( res->rguc_reserved, temprguc, sizeof temprguc ) )
			{
				MSG_WriteBits( 1, 1 );
				MSG_WriteBitData( res->rguc_reserved, 32 );
			}
			else
			{
				MSG_WriteBits( 0, 1 );
			}
		}

		if( sv_allowdownload->value > 0 && CustomResourcesList.size() )
		{
			for( CVector< resource_s* >::iterator iter = CustomResourcesList.begin(); iter != CustomResourcesList.end(); ++iter )
			{
				res = ( *iter );

				MSG_WriteBits( res->type, 4 );
				MSG_WriteBitString( res->szFileName );
				MSG_WriteBits( genericItemsCount + res->nIndex, 12 );
				MSG_WriteBits( res->nDownloadSize, 24 );
				MSG_WriteBits( res->ucFlags & 3, 3 );
				MSG_WriteBits( 0, 1 );
			}
		}
	}

	SV_SendConsistencyList();

	MSG_EndBitWriting( buf );
}
