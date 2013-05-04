
#ifndef PARSECONFIGS_H
#define PARSECONFIGS_H

#include "amxxmodule.h"

#include "CString.h"
#include "CVector.h"

const char ConfigDirectory			[] = "addons/http_resources_manager/configs";
const char ConfigDefaultResFileName	[] = "@default";

const uint32 MaxClients		= 32;
const uint32 MaxModLength	= 16;
const uint32 MaxUrlLength	= 128;
const uint32 MaxResLength	= 63;

enum FlagBits
{
	None			= 0,
	FatalIfMissing	= (1<<0),
	WasMissing		= (1<<1),
	Custom			= (1<<2),
	Requested		= (1<<3),
	Precached		= (1<<4)
};

enum
{
	MAP_DEFAULT = 1,
	MAP_PREFIX,
	MAP_NAME,
};

extern String ModName;
extern String ModuleStatus;
extern String ModuleConfigStatus;

extern bool MissingConfigs;
extern bool EngineConfigReady;
extern bool Initialized;
extern bool SendResourcesHookCreated;
extern bool NotifyClientDisconnectHookCreated;

extern CVector< resource_s* >	CustomResourcesList;
extern CVector< String >		CustomUrlsList;

extern String PlayerCurrentIp			[ MaxClients + 1 ];
extern time_t PlayerNextReconnectTime	[ MaxClients + 1 ];

extern cvar_t* CvarEnableDownloadFix;

void	handleCvars			( void );
void	handleConfig		( void );

String	getNextCustomUrl	( void );

void	populateList		( CVector< String >* list, CVector< String >* entries );
void	populateList		( CVector< resource_t* >* list, CVector< String >* entries );
void	retrieveFileEntries	( const char* file, CVector< String >* resList, CVector< String >* urlList );
bool	isEntryDuplicated	( const char* entry, CVector< String >* entriesList );

#endif // PARSECONFIGS_H

