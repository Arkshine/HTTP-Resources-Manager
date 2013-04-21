
#ifndef _CHOOKER_H_
#define _CHOOKER_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined __linux__

	#include <sys/mman.h>
	#include <dlfcn.h>
	#include <link.h>
	#include <limits.h>
	#include <unistd.h>

	#ifndef PAGESIZE
		#define PAGESIZE sysconf(_SC_PAGESIZE)
	#endif

	inline void* Align( void* address )
	{
		return ( void* )( ( long )address & ~( PAGESIZE - 1 ) );
	}

	inline uint32 IAlign( uint32 address )
	{
		return ( address & ~( PAGESIZE - 1 ) );
	}

	inline uint32 IAlign2( uint32 address )
	{
		return ( IAlign( address ) + PAGESIZE );
	}


	#define GET_EAX_POINTER(x) __asm volatile ("movl %%eax, %0" : "=m" (x):)

	const unsigned long PAGE_EXECUTE_READWRITE = PROT_READ | PROT_WRITE | PROT_EXEC;
	const unsigned long PAGE_READWRITE = PROT_READ | PROT_WRITE;

	static int dl_callback( struct dl_phdr_info *info, size_t size, void *data );

#else

	#include <Psapi.h>
	#include <windows.h>
	#include <io.h>

	#define GET_EAX_POINTER(x) __asm mov x, eax;
	#define PAGESIZE 4096

#endif

#define GET_ORIG_FUNC(x) CFunc *x; GET_EAX_POINTER(x);

typedef int BOOL;

enum SignatureEntryType
{ 
	SpecificByte, 
	AnyByteOrNothing, 
	AnyByte 
};

class CMemory
{
	public:

		unsigned char* signature;
		unsigned char* signatureData;

		int sigsize;

		char *baseadd;
		char *endadd;
		char *library;

		CMemory() : signature( 0 ), signatureData( 0 ), sigsize( 0 ), baseadd( ( char* )0xffffffff ), endadd( 0 ), library( 0 ) {}

		BOOL ChangeMemoryProtection( void* function, unsigned int size, unsigned long newProtection )
		{
			#ifdef __linux__

				void* alignedAddress = Align( function );
				return !mprotect( alignedAddress, size, newProtection );

			#else

				FlushInstructionCache( GetCurrentProcess(), function, size );

				static DWORD oldProtection;
				return VirtualProtect( function, size, newProtection, &oldProtection );

			#endif
		}

		BOOL ChangeMemoryProtection( void* address, unsigned int size, unsigned long newProtection, unsigned long &oldProtection )
		{
			#ifdef __linux__

				void* alignedAddress = Align( address );

				oldProtection = newProtection;

				return !mprotect( alignedAddress, size, newProtection );

			#else

				FlushInstructionCache( GetCurrentProcess(), address, size );

				return VirtualProtect( address, size, newProtection, &oldProtection );

			#endif
		}

		void SetupSignature( char *src )
		{
			int len = strlen( src );

			unsigned char *sig = new unsigned char[ len ];

			signature		= new unsigned char[ len ];
			signatureData	= new unsigned char[ len ];

			unsigned char *s1 = signature;
			unsigned char *s2 = signatureData;

			sigsize = 0;

			memcpy( sig, src, len + 1 );

			char *tok = strtok( ( char* )sig, "," );

			while( tok )
			{
				sigsize++;

				if( strstr( tok, "0x" ) )
				{
					*s1 = strtol( tok, NULL, 0 );
					*s2 = SpecificByte;

					*s1++;
					*s2++;
				}
				else
				{
					*s1 = 0xff;
					*s1++;

					switch( *tok )
					{
					case '*':
						{
							*s2 = AnyByte;
							break;
						}
					case '?':
						{
							*s2 = AnyByteOrNothing;
							break;
						}
					}

					*s2++;
				}

				tok = strtok( NULL, "," );
			}

			delete[] sig;
		}

		BOOL CompareSig( unsigned char *address, unsigned char *signature, unsigned char *signaturedata, int length )
		{
			if( length == 1 )
			{
				switch( *signaturedata )
				{
					case AnyByteOrNothing:
					case AnyByte:
					{
						return true;
					}
					case SpecificByte:
					{
						return ( *address == ( byte )*signature );
					}
				}
			}
			else
			{
				switch( *signaturedata )
				{
					case SpecificByte:
					{
						if( *address != ( byte )*signature )
							return false;
						else
							return CompareSig( address + 1, signature + 1, signaturedata + 1, length - 1 );
					}
					case AnyByteOrNothing:
					{
						if( CompareSig( address, signature + 1, signaturedata + 1, length - 1 ) ) 
							return true;
					}
					case AnyByte:
					{
						return CompareSig( address + 1, signature + 1, signaturedata + 1, length - 1 );
					}
				}

			}

			return true;
		}

		template <typename T>
		T SearchSignatureByAddress( T* p, char *sig, void* baseaddress, void* endaddress )
		{
			SetupSignature( sig );

			T ret = *p = NULL;

			unsigned char *start = ( unsigned char* )baseaddress;
			unsigned char *end = ( ( unsigned char* )endaddress ) - sigsize;

			unsigned int length = end - start ;

			if( ChangeMemoryProtection( start, length, PAGE_EXECUTE_READWRITE ) )
			{
				for( unsigned int i = 0; i <= length - sigsize; i++ )
				{
					if( CompareSig( start + i, signature, signatureData, sigsize ) )
					{
						*p = ret = (T)( start + i );
						break;
					}
				}
			}

			delete[] signature;
			delete[] signatureData;

			return ret;
		}

		template <typename T>
		T SearchSymbolByAddress( T* p, char *symbol, void *baseaddress )
		{
			void *handle = ( void* )0xffffffff;

			#if defined __linux__

				Dl_info info;

				if( baseaddress && dladdr( baseaddress, &info ) )
				{
					handle = dlopen( info.dli_fname, RTLD_NOW );
				}
				else if( !baseaddress )
				{
					handle = RTLD_DEFAULT;
				}

				if( handle >= 0 )
				{
					void *s = dlsym( handle, symbol );

					if( !dlerror() )
					{
						*p = (T)s;
						return (T)s;
					}
				}

			#else

				HMODULE module;

				if( GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, ( LPCSTR )baseaddress, &module ) )
				{
					void* s = GetProcAddress( module, symbol );

					if( s )
					{
						*p = (T)s;
						return (T)s;
					}
				}

			#endif

			return false;
		}

		template <typename T>
		T SearchSignatureByLibrary( T* p, char *sig, char *library )
		{
			return SearchByLibrary( p, sig, library, FALSE );
		}

		template <typename T>
		T SearchSymbolByLibrary( T* p, char *symbol, char *library )
		{
			return SearchByLibrary( p, symbol, library, TRUE );
		}
	
		template <typename T>
		T SearchByLibrary( T* p, char *data, char *library, BOOL symbol )
		{
			void *baseaddress = baseaddress = GetLibraryFromName( library );

			if( symbol )
				return SearchSymbolByAddress( p, data, baseaddress );
			else
				return SearchSignatureByAddress( p, data, baseadd, endadd );
		}

		template <typename T>
		T SearchByLibrary( T* p, char *data, void* address, BOOL symbol )
		{
			void *baseaddress = address;

			if( symbol )
				return SearchSymbolByAddress( p, data, baseaddress );
			else
				return SearchSignatureByAddress( p, data, baseadd, endadd );
		}

		void* GetLibraryFromAddress( void* addressContained )
		{
			#ifdef __linux__
			
				Dl_info info;

				if( addressContained && dladdr( addressContained, &info ) )
				{
					return GetLibraryFromName( ( char* )info.dli_fname );
				}
			
				return GetLibraryFromName( NULL );
	
			#else

				HMODULE module;

				if( GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, ( LPCSTR )addressContained, &module ) )
				{
					HANDLE process =  GetCurrentProcess();
					_MODULEINFO moduleInfo;

					if( GetModuleInformation( process, module, &moduleInfo, sizeof moduleInfo ) )
					{
						CloseHandle( process );

						baseadd = ( char* )moduleInfo.lpBaseOfDll;
						endadd = ( char* )( baseadd + moduleInfo.SizeOfImage );

						return ( void* )baseadd;
					}
				}

				return NULL;

			#endif
		}

		void* GetLibraryFromName( char *libname )
		{
			#ifdef __linux__

				library = libname;
				int baseaddress;

				if( ( baseaddress = dl_iterate_phdr( dl_callback, this ) ) )
				{
					return ( void* )baseaddress;
				}

			#else

				HMODULE hMods[ 1024 ];
				HANDLE	hProcess;
				DWORD	cbNeeded;

				unsigned int	i;
				static char		msg[100];

				hProcess = GetCurrentProcess();

				if( hProcess == NULL ) // IS NOT POSSIBLE!
					return NULL;

				if( EnumProcessModules( hProcess, hMods, sizeof( hMods ), &cbNeeded ) )
				{
					TCHAR szModName[ MAX_PATH ];
					_MODULEINFO info;

					for( i = 0; i < ( cbNeeded / sizeof( HMODULE ) ); i++ )
					{
						if( GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof( szModName ) / sizeof( TCHAR ) ) )
						{
							if( strstr( szModName, libname ) > 0 )
							{
								if( GetModuleInformation( hProcess, hMods[i], &info, sizeof( info ) ) )
								{
									baseadd = ( char* )info.lpBaseOfDll;
									endadd	= ( char* )( baseadd + info.SizeOfImage );

									return ( void* )baseadd;
								}
							}
						}
					}
				}

			#endif

			return NULL;
		}
};

class CFunc
{
	private:

		void* address;
		void* detour;

		CMemory* memFunc;

		unsigned char i_original[12];
		unsigned char i_patched[12];
		unsigned char *original;
		unsigned char *patched;

		bool ispatched;
		bool ishooked;

	public:

		CFunc( void* src, void* dst )
		{
			address		= src;
			detour		= dst;
			ishooked	= ispatched = 0;
			original	= &i_original[0];
			patched		= &i_patched[0];

			memFunc = new CMemory;
		};

		~CFunc() 
		{ 
			delete memFunc;
		};

		void *Hook( void *dst )
		{
			if( !ishooked && !ispatched )
			{
				unsigned int *p;
				detour = dst;

				memcpy( original, address, 12 );

				// lea    this ,%eax
				patched[0] = 0x8D;
				patched[1] = 0x05;

				p = ( unsigned int* )( patched + 2 );
				*p = ( unsigned int )this;

				// nop
				patched[6] = 0x90;

				// jmp detour
				patched[7] = 0xE9;
				p = ( unsigned int* )( patched + 8 );
				*p = ( unsigned int )dst - ( unsigned int )address - 12;

				if( Patch() )
				{
					return address;
				}

				ishooked = false;
			}

			return NULL;
		}

		void *GetOriginal()
		{
			return address;
		}

		bool Patch()
		{
			if( !ispatched )
			{
				if( memFunc->ChangeMemoryProtection( address, PAGESIZE, PAGE_EXECUTE_READWRITE ) )
				{
					memcpy( address, patched, 12 );
					ispatched = true;
				}
			}

			return ispatched;
		}

		bool Restore()
		{
			if( ispatched )
			{
				if( memFunc->ChangeMemoryProtection( address, PAGESIZE, PAGE_EXECUTE_READWRITE ) )
				{
					memcpy( address, original, 12 );
					ispatched = false;
				}
			}

			return !ispatched;
		}
};

class CHooker
{
	private:

		struct Obj
		{
			void*	src;
			CFunc*	func;
			Obj*	next;
		} *head;

	public:

		CMemory* memFunc;

		CHooker() : head( 0 ) 
		{
			memFunc = new CMemory;
		};

		~CHooker() 
		{ 
			Clear(); 
		};

		void Clear()
		{
			while( head )
			{
				Obj *obj = head->next;

				delete head->func;
				delete head;

				head = obj;
			}

			delete memFunc;
		}

		template <typename Tsrc, typename Tdst>
		CFunc* AddHook(Tsrc src, Tdst dst)
		{
			if( !src || !dst )
				return NULL;

			Obj *obj = head;

			if( !obj )
			{
				head = new Obj();
				obj = head;

				obj->src = ( void* )src;
				obj->func = new CFunc( ( void* )src, ( void* )dst);
				obj->next = NULL;
			}
			else
			{
				while( obj )
				{
					if( obj->src == ( void* )src )
					{
						break;
					}
					else if( !obj->next )
					{
						obj->next = new Obj();
						obj = obj->next;

						obj->src = ( void* )src;
						obj->func = new CFunc( ( void* )src, ( void* )dst );
						obj->next = NULL;

						break;
					}
					obj = obj->next;
				}
			}

			obj->func->Hook( ( void* )dst );

			return obj->func;
		}
};

#ifdef __linux__

	static int dl_callback( struct dl_phdr_info *info, size_t size, void *data )
	{
		CMemory* obj = ( CMemory* )data;

		if( ( !obj->library && !strlen( info->dlpi_name ) ) || strstr( info->dlpi_name, obj->library ) > 0 )
		{
			uint32 i = info->dlpi_phnum;

			for( --i; i > 0; i-- )
			{
				if( info->dlpi_phdr[i].p_memsz && IAlign( info->dlpi_phdr[i].p_vaddr ) )
				{
					if( ( uint32 )obj->baseadd > IAlign( info->dlpi_phdr[i].p_vaddr ) )
						obj->baseadd = ( char* )IAlign( info->dlpi_phdr[i].p_vaddr );
				
					if( ( uint32 )obj->endadd < ( info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz ) )
						obj->endadd = ( char* )IAlign2( ( info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz ) );
				}
			}

			return ( int )info->dlpi_addr;
		}

		return 0;
	}

#endif

#endif // _CHOOKER_H_
