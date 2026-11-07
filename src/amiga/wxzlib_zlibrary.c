/*
 * zlib's API for wx (and programs built with wx-config) on z.library
 * (GFOpenZlib, amicradle-guest libraries/openzlib). From 53.13 on its jump
 * table is the Aminet z.library's (z.lvo); the earlier stub here used
 * GFOpenZlib's old table, so on 53.13 inflateInit2 (-132) landed on adler32
 * and every gunzip (wxZlibInputStream) waited for ever. This is the same
 * stub KiCad links (gfamiga_zlib.c): it refuses GFOpenZlib older than 53.13
 * and computes the checksums itself when there is no z.library.
 * This file takes the place of wx's built-in zlib (libwxzlib).
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include "zlib.h"

static struct Library* ZBase;
static int             ZTried;
static int             ZOwn;

static void z_close( void )
{
    if( ZBase )
    {
        CloseLibrary( ZBase );
        ZBase = NULL;
    }
}

static struct Library* z_base( void )
{
    if( !ZTried )
    {
        ZTried = 1;
        ZBase = OpenLibrary( (CONST_STRPTR) "z.library", 0 );

        if( ZBase )
        {
            const char* id = (const char*) ZBase->lib_IdString;
            int gf = id && strstr( id, "GFOpenZlib" ) != NULL;

            /* GFOpenZlib before 53.13 had its own guessed jump table, not
             * the Aminet z.library's that these LVOs follow: don't use it */
            if( gf && ( ZBase->lib_Version < 53
                        || ( ZBase->lib_Version == 53 && ZBase->lib_Revision < 13 ) ) )
            {
                CloseLibrary( ZBase );
                ZBase = NULL;
            }
            else
            {
                /* the calls past -270 are GFOpenZlib's own */
                ZOwn = gf;
                atexit( z_close );
            }
        }
    }

    return ZBase;
}

#define CLOBBER "cc", "memory", "fp0", "fp1"

/* One call shape per register list in z.lvo. Every z.library call keeps
 * d2-d7 and a2-a6 (the AmigaOS convention); d0, d1, a0 and a1 are scratch. */
/* the offset must be a constant in the jsr, so each shape is a macro */
#define Z_A0( lvo, p0 )                                                                            \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" );                                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "=d"( _r ), "+a"( _a0 ) : "a"( _a6 )            \
                        : "d1", "a1", CLOBBER );                                                   \
        _r;                                                                                        \
    } )

#define Z_A0D0( lvo, p0, v0 )                                                                      \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+a"( _a0 ) : "a"( _a6 )            \
                        : "d1", "a1", CLOBBER );                                                   \
        _r;                                                                                        \
    } )

#define Z_A0D0D1( lvo, p0, v0, v1 )                                                                \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+a"( _a0 )            \
                        : "a"( _a6 ) : "a1", CLOBBER );                                            \
        _r;                                                                                        \
    } )

#define Z_A0A1( lvo, p0, p1 )                                                                      \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" );                                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "=d"( _r ), "+a"( _a0 ), "+a"( _a1 )            \
                        : "a"( _a6 ) : "d1", CLOBBER );                                            \
        _r;                                                                                        \
    } )

#define Z_A0A1D0( lvo, p0, p1, v0 )                                                                \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+a"( _a0 ), "+a"( _a1 )            \
                        : "a"( _a6 ) : "d1", CLOBBER );                                            \
        _r;                                                                                        \
    } )

#define Z_A0A1A2( lvo, p0, p1, p2 )                                                                \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" );                                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register APTR            _a2 __asm( "a2" ) = (APTR) ( p2 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "=d"( _r ), "+a"( _a0 ), "+a"( _a1 ),           \
                        "+a"( _a2 ) : "a"( _a6 ) : "d1", CLOBBER );                                \
        _r;                                                                                        \
    } )

#define Z_A0A1A2D0( lvo, p0, p1, p2, v0 )                                                          \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register APTR            _a2 __asm( "a2" ) = (APTR) ( p2 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+a"( _a0 ), "+a"( _a1 ),           \
                        "+a"( _a2 ) : "a"( _a6 ) : "d1", CLOBBER );                                \
        _r;                                                                                        \
    } )

#define Z_A0A1A2D0D1( lvo, p0, p1, p2, v0, v1 )                                                    \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register APTR            _a2 __asm( "a2" ) = (APTR) ( p2 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+a"( _a0 ),           \
                        "+a"( _a1 ), "+a"( _a2 ) : "a"( _a6 ) : CLOBBER );                         \
        _r;                                                                                        \
    } )

#define Z_A0A1A2A3( lvo, p0, p1, p2, p3 )                                                          \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" );                                                 \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register APTR            _a1 __asm( "a1" ) = (APTR) ( p1 );                                \
        register APTR            _a2 __asm( "a2" ) = (APTR) ( p2 );                                \
        register APTR            _a3 __asm( "a3" ) = (APTR) ( p3 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "=d"( _r ), "+a"( _a0 ), "+a"( _a1 ),           \
                        "+a"( _a2 ), "+a"( _a3 ) : "a"( _a6 ) : "d1", CLOBBER );                   \
        _r;                                                                                        \
    } )

#define Z_A0D0D1D2D3D4( lvo, p0, v0, v1, v2, v3, v4 )                                             \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register LONG            _d2 __asm( "d2" ) = (LONG) ( v2 );                                \
        register LONG            _d3 __asm( "d3" ) = (LONG) ( v3 );                                \
        register LONG            _d4 __asm( "d4" ) = (LONG) ( v4 );                                \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+d"( _d2 ),           \
                        "+d"( _d3 ), "+d"( _d4 ), "+a"( _a0 ) : "a"( _a6 ) : "a1", CLOBBER );      \
        _r;                                                                                        \
    } )

#define Z_A0D0D1D2D3( lvo, p0, v0, v1, v2, v3 )                                                   \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register LONG            _d2 __asm( "d2" ) = (LONG) ( v2 );                                \
        register LONG            _d3 __asm( "d3" ) = (LONG) ( v3 );                                \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+d"( _d2 ),           \
                        "+d"( _d3 ), "+a"( _a0 ) : "a"( _a6 ) : "a1", CLOBBER );                   \
        _r;                                                                                        \
    } )

#define Z_D0A0D1( lvo, v0, p0, v1 )                                                                \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register APTR            _a0 __asm( "a0" ) = (APTR) ( p0 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+a"( _a0 )            \
                        : "a"( _a6 ) : "a1", CLOBBER );                                            \
        _r;                                                                                        \
    } )

#define Z_D0( lvo, v0 )                                                                            \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ) : "a"( _a6 )                         \
                        : "d1", "a0", "a1", CLOBBER );                                             \
        _r;                                                                                        \
    } )

#define Z_D0D1D2( lvo, v0, v1, v2 )                                                                \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" ) = (LONG) ( v0 );                                 \
        register LONG            _d1 __asm( "d1" ) = (LONG) ( v1 );                                \
        register LONG            _d2 __asm( "d2" ) = (LONG) ( v2 );                                \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "+d"( _r ), "+d"( _d1 ), "+d"( _d2 )            \
                        : "a"( _a6 ) : "a0", "a1", CLOBBER );                                      \
        _r;                                                                                        \
    } )

#define Z_NONE( lvo )                                                                              \
    ( {                                                                                            \
        register LONG            _r __asm( "d0" );                                                 \
        register struct Library* _a6 __asm( "a6" ) = base;                                         \
        __asm volatile( "jsr %%a6@(" #lvo ":W)" : "=d"( _r ) : "a"( _a6 )                         \
                        : "d1", "a0", "a1", CLOBBER );                                             \
        _r;                                                                                        \
    } )

#define BASE_OR( fail )                                                                            \
    struct Library* base = z_base();                                                               \
    if( !base )                                                                                    \
        return fail;

/* a call only GFOpenZlib has (past the Aminet table) */
#define OWN_OR( fail )                                                                             \
    BASE_OR( fail )                                                                                \
    if( !ZOwn )                                                                                    \
        return fail;

/* zlib's version and stream checks, made here since z.library takes neither */
static int z_version_ok( const char* version, int stream_size )
{
    return version && version[0] == '1' && stream_size == (int) sizeof( z_stream );
}

/* ---- the calls, in z.lvo's order ---- */

const char* zlibVersion( void )
{
    return ZLIB_VERSION;
}

int deflateInit_( z_streamp strm, int level, const char* version, int stream_size )
{
    if( !z_version_ok( version, stream_size ) )
        return Z_VERSION_ERROR;

    if( !strm )
        return Z_STREAM_ERROR;

    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0D0( -36, strm, level );
}

int deflate( z_streamp strm, int flush )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0( -42, strm, flush );
}

int deflateEnd( z_streamp strm )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0( -48, strm );
}

int inflateInit_( z_streamp strm, const char* version, int stream_size )
{
    if( !z_version_ok( version, stream_size ) )
        return Z_VERSION_ERROR;

    if( !strm )
        return Z_STREAM_ERROR;

    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0( -54, strm );
}

int inflate( z_streamp strm, int flush )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0( -60, strm, flush );
}

int inflateEnd( z_streamp strm )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0( -66, strm );
}

int deflateInit2_( z_streamp strm, int level, int method, int windowBits, int memLevel,
                   int strategy, const char* version, int stream_size )
{
    if( !z_version_ok( version, stream_size ) )
        return Z_VERSION_ERROR;

    if( !strm )
        return Z_STREAM_ERROR;

    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0D0D1D2D3D4( -72, strm, level, method, windowBits, memLevel, strategy );
}

int deflateSetDictionary( z_streamp strm, const Bytef* dictionary, uInt dictLength )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1D0( -78, strm, dictionary, dictLength );
}

int deflateCopy( z_streamp dest, z_streamp source )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1( -84, dest, source );
}

int deflateReset( z_streamp strm )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0( -90, strm );
}

int deflateParams( z_streamp strm, int level, int strategy )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0D1( -96, strm, level, strategy );
}

int deflateTune( z_streamp strm, int good_length, int max_lazy, int nice_length, int max_chain )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0D1D2D3( -150, strm, good_length, max_lazy, nice_length, max_chain );
}

uLong deflateBound( z_streamp strm, uLong sourceLen )
{
    struct Library* base = z_base();

    if( !base ) /* zlib's own bound, only ever used to size a buffer */
        return sourceLen + ( sourceLen >> 12 ) + ( sourceLen >> 14 ) + ( sourceLen >> 25 ) + 13 + 6;

    return (uLong) Z_A0D0( -156, strm, sourceLen );
}

int deflatePending( z_streamp strm, unsigned* pending, int* bits )
{
    OWN_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1A2( -276, strm, pending, bits );
}

int deflatePrime( z_streamp strm, int bits, int value )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0D1( -162, strm, bits, value );
}

int deflateSetHeader( z_streamp strm, gz_headerp head )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1( -168, strm, head );
}

int inflateInit2_( z_streamp strm, int windowBits, const char* version, int stream_size )
{
    if( !z_version_ok( version, stream_size ) )
        return Z_VERSION_ERROR;

    if( !strm )
        return Z_STREAM_ERROR;

    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0D0( -102, strm, windowBits );
}

int inflateSetDictionary( z_streamp strm, const Bytef* dictionary, uInt dictLength )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1D0( -108, strm, dictionary, dictLength );
}

int inflateGetDictionary( z_streamp strm, Bytef* dictionary, uInt* dictLength )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1A2( -234, strm, dictionary, dictLength );
}

int inflateSync( z_streamp strm )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0( -144, strm );
}

int inflateCopy( z_streamp dest, z_streamp source )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1( -174, dest, source );
}

int inflateReset( z_streamp strm )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0( -114, strm );
}

int inflateReset2( z_streamp strm, int windowBits )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0( -252, strm, windowBits );
}

int inflatePrime( z_streamp strm, int bits, int value )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0D1( -180, strm, bits, value );
}

long inflateMark( z_streamp strm )
{
    OWN_OR( -( 1L << 16 ) )
    return (long) Z_A0( -282, strm );
}

/* z.library always checks the check value; turning the check off is a
 * request zlib may refuse too, so it is answered as done */
int inflateValidate( z_streamp strm, int check )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0D0( -258, strm, check );
}

int inflateGetHeader( z_streamp strm, gz_headerp head )
{
    BASE_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1( -186, strm, head );
}

uLong zlibCompileFlags( void )
{
    OWN_OR( 0 )
    return (uLong) Z_NONE( -288 );
}

int compress( Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen )
{
    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0A1A2D0( -120, dest, destLen, source, sourceLen );
}

int compress2( Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen, int level )
{
    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0A1A2D0D1( -222, dest, destLen, source, sourceLen, level );
}

uLong compressBound( uLong sourceLen )
{
    struct Library* base = z_base();

    if( !base )
        return sourceLen + ( sourceLen >> 12 ) + ( sourceLen >> 14 ) + ( sourceLen >> 25 ) + 13;

    return (uLong) Z_D0( -228, sourceLen );
}

int uncompress( Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen )
{
    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0A1A2D0( -126, dest, destLen, source, sourceLen );
}

int uncompress2( Bytef* dest, uLongf* destLen, const Bytef* source, uLong* sourceLen )
{
    BASE_OR( Z_MEM_ERROR )
    return (int) Z_A0A1A2A3( -246, dest, destLen, source, sourceLen );
}

/* the checksums: z.library's, or computed here without it */
static uLong adler_here( uLong adler, const Bytef* buf, z_size_t len )
{
    uLong a = adler & 0xffff, b = ( adler >> 16 ) & 0xffff;

    while( len )
    {
        z_size_t n = len < 5552 ? len : 5552;
        len -= n;

        while( n-- )
        {
            a += *buf++;
            b += a;
        }

        a %= 65521;
        b %= 65521;
    }

    return ( b << 16 ) | a;
}

static uLong crc_here( uLong crc, const Bytef* buf, z_size_t len )
{
    crc = ~crc & 0xffffffffUL;

    while( len-- )
    {
        crc ^= *buf++;

        for( int k = 0; k < 8; k++ )
            crc = ( crc >> 1 ) ^ ( 0xEDB88320UL & ( 0UL - ( crc & 1 ) ) );
    }

    return ~crc & 0xffffffffUL;
}

uLong adler32_z( uLong adler, const Bytef* buf, z_size_t len )
{
    struct Library* base = z_base();

    if( !buf )
        return 1;

    if( !base )
        return adler_here( adler, buf, len );

    while( len > 0x40000000UL )
    {
        adler = (uLong) Z_D0A0D1( -132, adler, buf, 0x40000000UL );
        buf += 0x40000000UL;
        len -= 0x40000000UL;
    }

    return (uLong) Z_D0A0D1( -132, adler, buf, len );
}

uLong adler32( uLong adler, const Bytef* buf, uInt len )
{
    return adler32_z( adler, buf, len );
}

uLong crc32_z( uLong crc, const Bytef* buf, z_size_t len )
{
    struct Library* base = z_base();

    if( !buf )
        return 0;

    if( !base )
        return crc_here( crc, buf, len );

    while( len > 0x40000000UL )
    {
        crc = (uLong) Z_D0A0D1( -138, crc, buf, 0x40000000UL );
        buf += 0x40000000UL;
        len -= 0x40000000UL;
    }

    return (uLong) Z_D0A0D1( -138, crc, buf, len );
}

uLong crc32( uLong crc, const Bytef* buf, uInt len )
{
    return crc32_z( crc, buf, len );
}

uLong adler32_combine( uLong adler1, uLong adler2, z_off_t len2 )
{
    BASE_OR( 0xffffffffUL )
    return (uLong) Z_D0D1D2( -210, adler1, adler2, len2 );
}

uLong crc32_combine( uLong crc1, uLong crc2, z_off_t len2 )
{
    BASE_OR( 0 )
    return (uLong) Z_D0D1D2( -216, crc1, crc2, len2 );
}

int deflateGetDictionary( z_streamp strm, Bytef* dictionary, uInt* dictLength )
{
    OWN_OR( Z_STREAM_ERROR )
    return (int) Z_A0A1A2( -294, strm, dictionary, dictLength );
}

const char* zError( int err )
{
    switch( err )
    {
    case Z_NEED_DICT:     return "need dictionary";
    case Z_STREAM_END:    return "stream end";
    case Z_OK:            return "";
    case Z_ERRNO:         return "file error";
    case Z_STREAM_ERROR:  return "stream error";
    case Z_DATA_ERROR:    return "data error";
    case Z_MEM_ERROR:     return "insufficient memory";
    case Z_BUF_ERROR:     return "buffer error";
    case Z_VERSION_ERROR: return "incompatible version";
    default:              return "";
    }
}
