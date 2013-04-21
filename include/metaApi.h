

#include <stddef.h>
#include <ctype.h>
#include "extdll.h"
#include "meta_api.h"
#include "osdep.h"

void		OnMetaAttach				( void );
void		OnMetaDetach				( void );
int			OnSpawn						( edict_t* pEntity );
qboolean	OnClientConnectPost			( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] );
void		OnServerActivatePost		( edict_t* pEdictList, int edictCount, int clientMax );
void		OnServerDeactivatePost		( void );

char* UTIL_VarArgs( char *format, ... );

#if defined _MSC_VER
	#pragma warning(disable:4103)  /* disable warning message 4103 that complains
	                                * about pragma pack in a header file */
	#pragma warning(disable:4100)  /* "'%$S' : unreferenced formal parameter" */

	#if _MSC_VER >= 1400
		#if !defined NO_MSVC8_AUTO_COMPAT

			/* Disable deprecation warnings concerning unsafe CRT functions */
			#if !defined _CRT_SECURE_NO_DEPRECATE
				#define _CRT_SECURE_NO_DEPRECATE
			#endif

			/* Replace the POSIX function with ISO C++ conformant ones as they are now deprecated */
			#define access _access
			#define cabs _cabs
			#define cgets _cgets
			#define chdir _chdir
			#define chmod _chmod
			#define chsize _chsize
			#define close _close
			#define cprintf _cprintf
			#define cputs _cputts
			#define creat _creat
			#define cscanf _cscanf
			#define cwait _cwait
			#define dup _dup
			#define dup2 _dup2
			#define ecvt _ecvt
			#define eof _eof
			#define execl _execl
			#define execle _execle
			#define execlp _execlp
			#define execlpe _execlpe
			#define execv _execv
			#define execve _execv
			#define execvp _execvp
			#define execvpe _execvpe
			#define fcloseall _fcloseall
			#define fcvt _fcvt
			#define fdopen _fdopen
			#define fgetchar _fgetchar
			#define filelength _filelength
			#define fileno _fileno
			#define flushall _flushall
			#define fputchar _fputchar
			#define gcvt _gcvt
			#define getch _getch
			#define getche _getche
			#define getcwd _getcwd
			#define getpid _getpid
			#define getw _getw
			#define hypot _hypot
			#define inp _inp
			#define inpw _inpw
			#define isascii __isascii
			#define isatty _isatty
			#define iscsym __iscsym
			#define iscsymf __iscsymf
			#define itoa _itoa
			#define j0 _j0
			#define j1 _j1
			#define jn _jn
			#define kbhit _kbhit
			#define lfind _lfind
			#define locking _locking
			#define lsearch _lsearch
			#define lseek _lseek
			#define ltoa _ltoa
			#define memccpy _memccpy
			#define memicmp _memicmp
			#define mkdir _mkdir
			#define mktemp _mktemp
			#define open _open
			#define outp _outp
			#define outpw _outpw
			#define putch _putch
			#define putenv _putenv
			#define putw _putw
			#define read _read
			#define rmdir _rmdir
			#define rmtmp _rmtmp
			#define setmode _setmode
			#define sopen _sopen
			#define spawnl _spawnl
			#define spawnle _spawnle
			#define spawnlp _spawnlp
			#define spawnlpe _spawnlpe
			#define spawnv _spawnv
			#define spawnve _spawnve
			#define spawnvp _spawnvp
			#define spawnvpe _spawnvpe
			#define strcmpi _strcmpi
			#define strdup _strdup
			#define stricmp _stricmp
			#define strlwr _strlwr
			#define strnicmp _strnicmp
			#define strnset _strnset
			#define strrev _strrev
			#define strset _strset
			#define strupr _strupr
			#define swab _swab
			#define tell _tell
			#define tempnam _tempnam
			#define toascii __toascii
			#define tzset _tzset
			#define ultoa _ultoa
			#define umask _umask
			#define ungetch _ungetch
			#define unlink _unlink
			#define wcsdup _wcsdup
			#define wcsicmp _wcsicmp
			#define wcsicoll _wcsicoll
			#define wcslwr _wcslwr
			#define wcsnicmp _wcsnicmp
			#define wcsnset _wcsnset
			#define wcsrev _wcsrev
			#define wcsset _wcsset
			#define wcsupr _wcsupr
			#define write _write
			#define y0 _y0
			#define y1 _y1
			#define yn _yn

			/* Disable deprecation warnings because MSVC8 seemingly thinks the ISO C++ conformant 
			 * functions above are deprecated. */
			#pragma warning (disable:4996)
				
		#endif
	#else
		#define vsnprintf _vsnprintf
	#endif
#endif

typedef int32 cell;