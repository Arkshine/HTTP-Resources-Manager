
#ifndef ENGINECONFIG_H
#define ENGINECONFIG_H

#include "amxxmodule.h"
#include "engineStructs.h"
#include "CString.h"
#include "chooker.h"

#if defined __linux__

	#define FUNC_SV_SENDRESOURCES_DS				"SV_SendResources"
	#define FUNC_SV_SENDCONSISTENCYLIST_DS			"SV_SendConsistencyList"
	#define FUNC_MSG_WRITEBITS_DS					"MSG_WriteBits"
	#define FUNC_MSG_WRITEBITSTRING_DS				"MSG_WriteBitString"
	#define FUNC_MSG_STARTBITWRITING_DS				"MSG_StartBitWriting"
	#define FUNC_MSG_ENDBITWRITING_DS				"MSG_EndBitWriting"
	#define FUNC_STEAM_NOTIFYCLIENTDISCONNECT_DS	"Steam_NotifyClientDisconnect"
	#define FUNC_SV_DS								"sv"

#else

	#define FUNC_SV_SENDRESOURCES_DS				"0x55,0x8B,*,0x83,*,*,0x53,0x57,0x6A,*,0x8D"
	#define FUNC_SV_SENDCONSISTENCYLIST_DS			"0x55,0x8B,*,0x51,0x8B,*,*,*,*,*,0xA1,*,*,*,*,0x53,0x57"
	#define FUNC_MSG_WRITEBITS_DS					"0x55,0x8B,*,0x8B,*,*,0x53,0x56,0x33,*,0x57,0x8B"
	#define FUNC_MSG_WRITEBITSTRING_DS				"0x55,0x8B,*,0x56,0x8B,*,*,0x8A,*,0x84,*,0x74,*,0x0F,*,*,0x6A,*,0x50"
	#define FUNC_MSG_STARTBITWRITING_DS				"0x55,0x8B,*,0x8B,*,*,0xC7,*,*,*,*,*,*,*,*,*,0xA3,*,*,*,*,0x8B"
	#define FUNC_MSG_ENDBITWRITING_DS				"0x8B,*,*,*,*,*,0xF6,*,*,*,0x75,*,0xA1"
	#define FUNC_STEAM_NOTIFYCLIENTDISCONNECT_DS	"0x55,0x8B,*,0xA1,*,*,*,*,0x85,*,0x74,*,0x8B,*,*,0x50,0xE8,*,*,*,*,0x8B"
	#define FUNC_HOST_ISSERVERACTIVE_DS				"0xA1,*,*,*,*,0xC3,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x55,0x8B,*,0x81,*,*,*,*,*,0x53,0x56,0x33"

#endif

typedef void	( *FuncSV_SendResources )				( sizebuf_t* );
typedef void	( *FuncSV_SendConsistencyList )			( void );
typedef void	( *FuncMSG_WriteBits )					( uint32, int );
typedef void	( *FuncMSG_StartBitWriting )			( sizebuf_t* );
typedef void	( *FuncMSG_EndBitWriting )				( sizebuf_t* );
typedef void	( *FuncSteam_NotifyClientDisconnect )	( client_t* );

extern FuncSV_SendResources				SV_SendResources;
extern FuncSV_SendConsistencyList		SV_SendConsistencyList;
extern FuncMSG_WriteBits				MSG_WriteBits;
extern FuncMSG_StartBitWriting			MSG_StartBitWriting;
extern FuncMSG_EndBitWriting			MSG_EndBitWriting;
extern FuncSteam_NotifyClientDisconnect	Steam_NotifyClientDisconnect;

extern CFunc* NotifyClientDisconnectHook;

extern server_s* sv;

extern String	ServerLocalIp;
extern String	ServerInternetIp;
extern uint32	CurrentServerSpawnCount;

void handleEngineConfig		( void );
bool findFunctions			( void );
bool findServerStructAddress( void );
bool createSendResourcesHook( void );

void OnSV_SendResources				( sizebuf_t* buf );
void OnSteam_NotifyClientDisconnect	( client_t* cl );

#endif // ENGINECONFIG_H