
#ifndef ENGINEUTILS_H
#define ENGINEUTILS_H

#include "engineStructs.h"

const int SVC_RESOURCELIST		= 43;
const int SVC_RESOURCEREQUEST	= 45;
const int SVC_RESOURCELOCATION	= 56;

void	SZ_Clear	( sizebuf_t *buf );
void*	SZ_GetSpace	( sizebuf_t* buf, uint32 length ) ;
void	SZ_Write	( sizebuf_t *buf, const void *data, int length );

void	MSG_WriteByte		( sizebuf_t *sb, uint32 c );
void	MSG_WriteLong		( sizebuf_t *sb, uint32 c );
void	MSG_WriteShort		( sizebuf_t *sb, int c );
void	MSG_WriteString		( sizebuf_t *sb, const char *s );
void	MSG_WriteBitString	( const char* str );
void	MSG_WriteBitData	( unsigned char	* str, int size );

#endif // ENGINEUTILS_H
