/*
 * zlib's gzip file calls (gzopen and the others) for wx's zlib.h, on the
 * stream calls of wxzlib_zlibrary.c (z.library). z.library has no file
 * calls (the Aminet one has none either), so a program that reads or
 * writes .gz files through zlib's gz* API (Golly's patterns) gets them
 * here, in libwxzlib, with libnix's stdio underneath.
 *
 * Reading takes a gzip file (members one after another, as gunzip does)
 * or, as zlib does, a plain file as it is. Writing makes one gzip member.
 * No seeking but gzrewind on a file being read; "a" appends a new member.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

#define GZ_BUF 16384

typedef struct
{
    struct gzFile_s x;          /* zlib.h's gzgetc() macro reads this: have stays 0 */
    FILE*          f;
    int            writing;
    int            raw;         /* reading a file that is not gzip */
    int            eof;         /* no more uncompressed data */
    int            err;
    int            level, strategy;
    long           start;       /* where the data starts (gzrewind) */
    z_off_t        pos;         /* uncompressed bytes read or written */
    z_stream       z;
    unsigned char  in[GZ_BUF];
    unsigned char  out[GZ_BUF];
} gz_t;

static int gz_fill( gz_t* g )
{
    size_t n;
    if ( g->z.avail_in )
        return 1;
    n = fread( g->in, 1, GZ_BUF, g->f );
    if ( n == 0 )
    {
        if ( ferror( g->f ) )
            g->err = Z_ERRNO;
        return 0;
    }
    g->z.next_in = g->in;
    g->z.avail_in = (uInt)n;
    return 1;
}

static int gz_start_read( gz_t* g )
{
    memset( &g->z, 0, sizeof g->z );
    g->z.next_in = g->in;
    g->z.avail_in = 0;
    g->start = ftell( g->f );
    gz_fill( g );
    if ( g->z.avail_in >= 2 && g->in[0] == 0x1f && g->in[1] == 0x8b )
        return inflateInit2( &g->z, 15 + 16 ) == Z_OK;
    g->raw = 1;
    return 1;
}

gzFile ZEXPORT gzopen( const char* path, const char* mode )
{
    gz_t* g;
    const char* m;
    char fm[3] = "rb";

    if ( path == NULL || mode == NULL )
        return NULL;
    g = (gz_t*)calloc( 1, sizeof *g );
    if ( g == NULL )
        return NULL;
    g->level = Z_DEFAULT_COMPRESSION;
    g->strategy = Z_DEFAULT_STRATEGY;
    g->writing = -1;
    for ( m = mode; *m; m++ )
    {
        if ( *m == 'r' )
            g->writing = 0;
        else if ( *m == 'w' || *m == 'a' )
        {
            g->writing = 1;
            fm[0] = *m;
        }
        else if ( *m >= '0' && *m <= '9' )
            g->level = *m - '0';
        else if ( *m == 'f' )
            g->strategy = Z_FILTERED;
        else if ( *m == 'h' )
            g->strategy = Z_HUFFMAN_ONLY;
        else if ( *m == 'R' )
            g->strategy = Z_RLE;
        else if ( *m == 'F' )
            g->strategy = Z_FIXED;
    }
    if ( g->writing < 0 || ( g->f = fopen( path, fm ) ) == NULL )
    {
        free( g );
        return NULL;
    }
    if ( g->writing )
    {
        if ( deflateInit2( &g->z, g->level, Z_DEFLATED, 15 + 16, 8, g->strategy ) != Z_OK )
            goto fail;
        g->z.next_out = g->out;
        g->z.avail_out = GZ_BUF;
    }
    else if ( !gz_start_read( g ) )
        goto fail;
    return (gzFile)g;

fail:
    fclose( g->f );
    free( g );
    return NULL;
}

/* the output buffer to the file */
static int gz_out( gz_t* g )
{
    size_t n = GZ_BUF - g->z.avail_out;
    if ( n && fwrite( g->out, 1, n, g->f ) != n )
        return g->err = Z_ERRNO;
    g->z.next_out = g->out;
    g->z.avail_out = GZ_BUF;
    return Z_OK;
}

/* deflates the input; Z_NO_FLUSH keeps a part-filled output buffer for
 * the next call, the flushes write everything out */
static int gz_deflate( gz_t* g, int flush )
{
    for ( ;; )
    {
        int r = deflate( &g->z, flush );
        int full;
        if ( r == Z_STREAM_ERROR )
            return g->err = r;
        full = g->z.avail_out == 0;
        if ( ( full || flush != Z_NO_FLUSH ) && gz_out( g ) != Z_OK )
            return g->err;
        if ( flush == Z_NO_FLUSH )
        {
            if ( !full && g->z.avail_in == 0 )
                return Z_OK;
        }
        else if ( flush == Z_FINISH )
        {
            if ( r == Z_STREAM_END )
                return Z_OK;
        }
        else if ( !full )
            return Z_OK;
    }
}

int ZEXPORT gzwrite( gzFile file, voidpc buf, unsigned len )
{
    gz_t* g = (gz_t*)file;
    if ( g == NULL || g->writing != 1 || g->err )
        return 0;
    if ( len == 0 )
        return 0;
    g->z.next_in = (Bytef*)buf;
    g->z.avail_in = len;
    if ( gz_deflate( g, Z_NO_FLUSH ) != Z_OK )
        return 0;
    g->pos += len;
    return (int)len;
}

int ZEXPORT gzputc( gzFile file, int c )
{
    unsigned char ch = (unsigned char)c;
    return gzwrite( file, &ch, 1 ) == 1 ? ch : -1;
}

int ZEXPORT gzputs( gzFile file, const char* s )
{
    size_t n = strlen( s );
    if ( n == 0 )
        return 0;
    return gzwrite( file, s, (unsigned)n ) == (int)n ? (int)n : -1;
}

int ZEXPORT gzflush( gzFile file, int flush )
{
    gz_t* g = (gz_t*)file;
    if ( g == NULL || g->writing != 1 )
        return Z_STREAM_ERROR;
    if ( g->err )
        return g->err;
    if ( gz_deflate( g, flush ) != Z_OK )
        return g->err;
    if ( fflush( g->f ) )
        return g->err = Z_ERRNO;
    return Z_OK;
}

int ZEXPORT gzread( gzFile file, voidp buf, unsigned len )
{
    gz_t* g = (gz_t*)file;
    unsigned char* p = (unsigned char*)buf;
    unsigned got = 0;

    if ( g == NULL || g->writing != 0 || ( g->err && g->err != Z_BUF_ERROR ) )
        return -1;
    while ( got < len && !g->eof )
    {
        if ( g->raw )
        {
            /* what the gzip check read first, then the file itself */
            if ( g->z.avail_in )
            {
                unsigned n = g->z.avail_in < len - got ? g->z.avail_in : len - got;
                memcpy( p + got, g->z.next_in, n );
                g->z.next_in += n;
                g->z.avail_in -= n;
                got += n;
            }
            else
            {
                size_t n = fread( p + got, 1, len - got, g->f );
                if ( n == 0 )
                {
                    if ( ferror( g->f ) )
                        g->err = Z_ERRNO;
                    g->eof = 1;
                }
                got += (unsigned)n;
            }
            continue;
        }
        if ( g->z.avail_in == 0 && !gz_fill( g ) )
        {
            if ( !g->err )
                g->err = Z_BUF_ERROR;   /* the file ends inside a member */
            g->eof = 1;
            break;
        }
        g->z.next_out = p + got;
        g->z.avail_out = len - got;
        {
            int r = inflate( &g->z, Z_NO_FLUSH );
            got = len - g->z.avail_out;
            if ( r == Z_STREAM_END )
            {
                /* another member after this one? */
                if ( g->z.avail_in == 0 )
                    gz_fill( g );
                if ( g->z.avail_in >= 2 && g->z.next_in[0] == 0x1f && g->z.next_in[1] == 0x8b )
                    inflateReset( &g->z );
                else
                    g->eof = 1;
            }
            else if ( r != Z_OK && r != Z_BUF_ERROR )
            {
                g->err = r;
                g->eof = 1;
                if ( got == 0 )
                    return -1;
            }
        }
    }
    g->pos += got;
    return (int)got;
}

int ZEXPORT gzgetc( gzFile file )
{
    unsigned char c;
    return gzread( file, &c, 1 ) == 1 ? c : -1;
}

int ZEXPORT gzgetc_( gzFile file )
{
    return gzgetc( file );
}

char* ZEXPORT gzgets( gzFile file, char* buf, int len )
{
    int i = 0, c = 0;
    if ( buf == NULL || len < 1 )
        return NULL;
    while ( i < len - 1 && ( c = gzgetc( file ) ) != -1 )
    {
        buf[i++] = (char)c;
        if ( c == '\n' )
            break;
    }
    buf[i] = '\0';
    return i == 0 ? NULL : buf;
}

int ZEXPORT gzeof( gzFile file )
{
    gz_t* g = (gz_t*)file;
    return g != NULL && g->writing == 0 && g->eof;
}

int ZEXPORT gzrewind( gzFile file )
{
    gz_t* g = (gz_t*)file;
    if ( g == NULL || g->writing != 0 )
        return -1;
    if ( !g->raw )
        inflateEnd( &g->z );
    if ( fseek( g->f, g->start, SEEK_SET ) )
        return -1;
    g->raw = g->eof = g->err = 0;
    g->pos = 0;
    return gz_start_read( g ) ? 0 : -1;
}

z_off_t ZEXPORT gztell( gzFile file )
{
    gz_t* g = (gz_t*)file;
    return g == NULL ? -1 : g->pos;
}

z_off_t ZEXPORT gzoffset( gzFile file )
{
    gz_t* g = (gz_t*)file;
    long at;
    if ( g == NULL || ( at = ftell( g->f ) ) < 0 )
        return -1;
    if ( g->writing )
        return (z_off_t)( at + ( GZ_BUF - g->z.avail_out ) );
    return (z_off_t)( at - g->z.avail_in );
}

const char* ZEXPORT gzerror( gzFile file, int* errnum )
{
    gz_t* g = (gz_t*)file;
    int e = g == NULL ? Z_STREAM_ERROR : g->err;
    if ( errnum )
        *errnum = e;
    return e == Z_OK ? "" : e == Z_ERRNO ? "file error" : e == Z_BUF_ERROR ? "unexpected end of file"
         : e == Z_DATA_ERROR ? "not gzip data" : "error";
}

void ZEXPORT gzclearerr( gzFile file )
{
    gz_t* g = (gz_t*)file;
    if ( g )
    {
        g->err = 0;
        if ( g->writing == 0 && !feof( g->f ) )
            g->eof = 0;
        clearerr( g->f );
    }
}

int ZEXPORT gzclose( gzFile file )
{
    gz_t* g = (gz_t*)file;
    int r = Z_OK;
    if ( g == NULL )
        return Z_STREAM_ERROR;
    if ( g->writing )
    {
        g->z.avail_in = 0;
        if ( !g->err )
            gz_deflate( g, Z_FINISH );
        deflateEnd( &g->z );
        r = g->err ? g->err : Z_OK;
    }
    else if ( !g->raw )
        inflateEnd( &g->z );
    if ( fclose( g->f ) )
        r = Z_ERRNO;
    free( g );
    return r;
}

int ZEXPORT gzclose_r( gzFile file ) { return gzclose( file ); }
int ZEXPORT gzclose_w( gzFile file ) { return gzclose( file ); }
