#define _CRT_SECURE_NO_WARNINGS

#include <string.h>
#include <new>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "metaApi.h"

#undef DLLEXPORT
#ifndef __linux__
	#define DLLEXPORT __declspec(dllexport)
#else
	#define DLLEXPORT __attribute__((visibility("default")))
	#define _LINUX
#endif

#undef C_DLLEXPORT
#define C_DLLEXPORT extern "C" DLLEXPORT


#define MODULE_VERSION		"1.0"
#define MODULE_VERSION_F	1.0

#define MODULE_LOADABLE		PT_STARTUP
#define MODULE_UNLOADABLE	PT_NEVER

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,			// ifvers
	"HTTP Resource Manager",		// name
	MODULE_VERSION,					// version
	__DATE__,						// date
	"Arkshine",						// author
	"http://forums.alliedmods.net",	// url
	"HTTP-RM",						// logtag, all caps please
	PT_ANYTIME,						// (when) loadable
	PT_ANYTIME,						// (when) unloadable
};

meta_globals_t	*gpMetaGlobals;		// metamod globals
gamedll_funcs_t *gpGamedllFuncs;	// gameDLL function tables
mutil_funcs_t	*gpMetaUtilFuncs;	// metamod utility functions

enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

static META_FUNCTIONS gMetaFunctionTable =
{
	NULL,				// pfnGetEntityAPI				HL SDK; called before game DLL
	NULL,				// pfnGetEntityAPI_Post			META; called after game DLL
	GetEntityAPI2,		// pfnGetEntityAPI2				HL SDK2; called before game DLL
	GetEntityAPI2_Post,	// pfnGetEntityAPI2_Post		META; called after game DLL
	NULL,				// pfnGetNewDLLFunctions		HL SDK2; called before game DLL
	NULL,				// pfnGetNewDLLFunctions_Post	META; called after game DLL
	NULL,				// pfnGetEngineFunctions		META; called before HL engine
	NULL,				// pfnGetEngineFunctions_Post	META; called after HL engine
};

#define COMMAND_CONTINUE	0
#define COMMAND_HANDLED		1

char* UTIL_VarArgs( char *format, ... )
{
	va_list		argptr;
	static char		string[1024];

	va_start (argptr, format);
	vsnprintf (string, sizeof(string), format, argptr);
	va_end (argptr);

	return string;
}

DLL_FUNCTIONS gpFunctionTable;
C_DLLEXPORT int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int * )
{
	memset( &gpFunctionTable, 0, sizeof( DLL_FUNCTIONS ) );

	gpFunctionTable.pfnSpawn = OnSpawn;

	memcpy( pFunctionTable, &gpFunctionTable, sizeof( DLL_FUNCTIONS ) );

	return 1;
}

DLL_FUNCTIONS gpFunctionTablePost;
C_DLLEXPORT int GetEntityAPI2_Post( DLL_FUNCTIONS *pFunctionTable, int * )
{
	memset( &gpFunctionTable, 0, sizeof( DLL_FUNCTIONS ) );

	gpFunctionTablePost.pfnClientConnect	= OnClientConnectPost;
	gpFunctionTablePost.pfnServerActivate	= OnServerActivatePost;
	gpFunctionTablePost.pfnServerDeactivate	= OnServerDeactivatePost;

	memcpy( pFunctionTable, &gpFunctionTablePost, sizeof( DLL_FUNCTIONS ) );

	return 1;
}

C_DLLEXPORT int Meta_Query( char *ifvers, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs )
{
	*pPlugInfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;

	int	mmajor = 0, mminor = 0,	pmajor = 0, pminor = 0;

	sscanf( ifvers, "%d:%d", &mmajor, &mminor );
	sscanf( Plugin_info.ifvers, "%d:%d", &pmajor, &pminor );

	if( strcmp( ifvers, Plugin_info.ifvers ) )
	{
		if( pmajor > mmajor )
		{
			LOG_ERROR(PLID, "metamod version is too old for this plugin; update metamod");
			return FALSE;
		}
		else if( pmajor < mmajor )
		{
			LOG_ERROR(PLID, "metamod version is incompatible with this plugin; please find a newer version of this plugin");
			return FALSE;
		}
		else if( pmajor == mmajor )
		{
			if( pminor > mminor )
			{
				LOG_ERROR( PLID, "metamod version is too old for this plugin; update metamod" );
				return FALSE;
			}
			else if( pminor < mminor )
			{
				LOG_MESSAGE( PLID, "warning: meta-interface version mismatch (plugin: %s, metamod: %s)", Plugin_info.ifvers, ifvers );
			}
		}
	}

	return TRUE;
}


C_DLLEXPORT int Meta_Attach( PLUG_LOADTIME  now, META_FUNCTIONS *pFunctionTable,
							meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs )
{
	if( now > MODULE_LOADABLE )
	{
		LOG_ERROR( PLID, "Can't load plugin right now" );
		return FALSE;
	}

	if( !pMGlobals )
	{
		LOG_ERROR( PLID, "Meta_Attach called with null pMGlobals" );
		return FALSE;
	}

	gpMetaGlobals = pMGlobals;

	if( !pFunctionTable )
	{
		LOG_ERROR( PLID, "Meta_Attach called with null pFunctionTable" );
		return FALSE;
	}

	memcpy( pFunctionTable, &gMetaFunctionTable, sizeof( META_FUNCTIONS ) );
	gpGamedllFuncs = pGamedllFuncs;

	OnMetaAttach();

	return TRUE;
}

C_DLLEXPORT int Meta_Detach( PLUG_LOADTIME now, PL_UNLOAD_REASON reason )
{
	if( now > MODULE_UNLOADABLE && reason != PNL_CMD_FORCED )
	{
		LOG_ERROR( PLID, "Can't unload plugin right now" );
		return FALSE;
	}

	OnMetaDetach();

	return TRUE;
}

#ifdef __linux__
// linux prototype
	C_DLLEXPORT void GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals ) {
#else
	#ifdef _MSC_VER
	// MSVC: Simulate __stdcall calling convention
	C_DLLEXPORT __declspec(naked) void GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
	{
		__asm			// Prolog
		{
			// Save ebp
			push		ebp
			// Set stack frame pointer
			mov			ebp, esp
			// Allocate space for local variables
			// The MSVC compiler gives us the needed size in __LOCAL_SIZE.
			sub			esp, __LOCAL_SIZE
			// Push registers
			push		ebx
			push		esi
			push		edi
		}
	#else	// _MSC_VER
		#ifdef __GNUC__
			// GCC can also work with this
			C_DLLEXPORT void __stdcall GiveFnptrsToDll( enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
			{
		#else	// __GNUC__
			// compiler not known
			#error There is no support (yet) for your compiler. Please use MSVC or GCC compilers or contact the AMX Mod X dev team.
		#endif	// __GNUC__
	#endif // _MSC_VER
#endif // __linux__

		// ** Function core <--
		memcpy( &g_engfuncs, pengfuncsFromEngine, sizeof( enginefuncs_t ) );
		gpGlobals = pGlobals;

		// NOTE!  Have to call logging function _after_ copying into g_engfuncs, so
		// that g_engfuncs.pfnAlertMessage() can be resolved properly, heh. :)
		// UTIL_LogPrintf("[%s] dev: called: GiveFnptrsToDll\n", Plugin_info.logtag);
		// --> ** Function core

		#ifdef _MSC_VER
		// Epilog
		if (sizeof(int*) == 8)
		{	// 64 bit
			__asm
			{
				// Pop registers
				pop	edi
					pop	esi
					pop	ebx
					// Restore stack frame pointer
					mov	esp, ebp
					// Restore ebp
					pop	ebp
					// 2 * sizeof(int*) = 16 on 64 bit
					ret 16
			}
		}
		else
		{	// 32 bit
			__asm
			{
				// Pop registers
				pop	edi
					pop	esi
					pop	ebx
					// Restore stack frame pointer
					mov	esp, ebp
					// Restore ebp
					pop	ebp
					// 2 * sizeof(int*) = 8 on 32 bit
					ret 8
			}
		}
		#endif // #ifdef _MSC_VER
	}