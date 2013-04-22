
#include "engineConfigs.h"
#include "engineCvars.h"
#include "moduleConfigs.h"

CHooker		HookerRef;
CHooker*	Hooker = &HookerRef;

FuncSV_SendResources				SV_SendResources				= NULL;
FuncSV_SendConsistencyList			SV_SendConsistencyList			= NULL;
FuncMSG_WriteBits					MSG_WriteBits					= NULL;
FuncMSG_StartBitWriting				MSG_StartBitWriting				= NULL;
FuncMSG_EndBitWriting				MSG_EndBitWriting				= NULL;
FuncSteam_NotifyClientDisconnect	Steam_NotifyClientDisconnect	= NULL;

#if defined _WIN32
	void* Host_IsServerActive = NULL;
#endif 

server_s* sv = NULL;

String ServerLocalIp;
String ServerInternetIp;

uint32 CurrentServerSpawnCount = 0;

void handleEngineConfig( void )
{
	sv_downloadurl	= CVAR_GET_POINTER( "sv_downloadurl" );
	sv_allowdownload= CVAR_GET_POINTER( "sv_allowdownload" );
	developer		= CVAR_GET_POINTER( "developer" );

	ModuleStatus.assign( UTIL_VarArgs( "\n %s v%s, by %s.\n -\n", MODULE_NAME, MODULE_VERSION, MODULE_AUTHOR ) );
	ModuleStatus.append( " Memory initialization started.\n" );

	EngineConfigReady = findFunctions()				&& 
						findServerStructAddress()	&& 
						createSendResourcesHook();

	ModuleStatus.append
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
	ModuleStatus.append( UTIL_VarArgs( "\n\t[%s] Finding \"Host_IsServerActive\" function", Host_IsServerActive ? "FOUND" : "NOT FOUND" ) );
#endif
	ModuleStatus.append( UTIL_VarArgs( "\n\t[%s] Finding \"sv\" global variable"					, sv != NULL						? "FOUND"	: "NOT FOUND" ) );
	ModuleStatus.append( UTIL_VarArgs( "\n\n\t[%s] Creating \"SV_SendResourcesHook\" hook"			, SendResourcesHookCreated			? "SUCCESS" : "FAILED" ) );
	ModuleStatus.append( UTIL_VarArgs( "\n\t[%s] Creating \"Steam_NotifyClientDisconnect\" hook\n\n", NotifyClientDisconnectHookCreated ? "SUCCESS" : "FAILED" ) );
	ModuleStatus.append( " Memory initialization ended.\n\n" );

	printf( ModuleStatus.c_str() );
}


bool findFunctions( void )
{
	void* engineAddress = Hooker->memFunc->GetLibraryFromAddress( ( void* )gpGlobals );

	#if defined __linux__
		BOOL useSymbol = TRUE;
	#else
		BOOL useSymbol = FALSE;
	#endif

	SV_SendResources		= Hooker->MemorySearch< FuncSV_SendResources >		( FUNC_SV_SENDRESOURCES_DS		, engineAddress, useSymbol );
	SV_SendConsistencyList	= Hooker->MemorySearch< FuncSV_SendConsistencyList >( FUNC_SV_SENDCONSISTENCYLIST_DS, engineAddress, useSymbol );
	MSG_WriteBits			= Hooker->MemorySearch< FuncMSG_WriteBits >			( FUNC_MSG_WRITEBITS_DS			, engineAddress, useSymbol );
	MSG_StartBitWriting		= Hooker->MemorySearch< FuncMSG_StartBitWriting >	( FUNC_MSG_STARTBITWRITING_DS	, engineAddress, useSymbol );
	MSG_EndBitWriting		= Hooker->MemorySearch< FuncMSG_EndBitWriting >		( FUNC_MSG_ENDBITWRITING_DS		, engineAddress, useSymbol );

	Steam_NotifyClientDisconnect = Hooker->MemorySearch<FuncSteam_NotifyClientDisconnect>( FUNC_STEAM_NOTIFYCLIENTDISCONNECT_DS, engineAddress, useSymbol );

	#if defined __linux__

	return	SV_SendResources				&& 
			SV_SendConsistencyList			&& 
			MSG_WriteBits					&& 
			MSG_StartBitWriting				&& 
			MSG_EndBitWriting				&& 
			Steam_NotifyClientDisconnect;

	#else

	Host_IsServerActive = Hooker->MemorySearch< void* >( FUNC_HOST_ISSERVERACTIVE_DS, engineAddress, FALSE );

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
		sv = Hooker->MemorySearch< server_s* >( FUNC_SV_DS, ( void* )gpGlobals, TRUE );
	#else
		sv = ( server_s* )*( unsigned long* )( ( unsigned char* )Host_IsServerActive + 1 );
	#endif

	return sv != NULL;
}

bool createSendResourcesHook( void )
{
	if( SV_SendResources )
	{
		SendResourcesHookCreated = Hooker->CreateHook( ( void* )SV_SendResources, ( void* )OnSV_SendResources, TRUE ) ? true : false;
	}

	if( Steam_NotifyClientDisconnect )
	{
		NotifyClientDisconnectHookCreated = Hooker->CreateHook( ( void* )Steam_NotifyClientDisconnect, ( void* )OnSteam_NotifyClientDisconnect, TRUE ) ? true : false;
	}

	return SendResourcesHookCreated && NotifyClientDisconnectHookCreated;
}


