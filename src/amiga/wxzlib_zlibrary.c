/*
 * zlib's API for wx (and programs built with wx-config) on z.library
 * (GFOpenZlib): each call is the library's own, one LVO each, with the
 * stream in A0 (amicradle-guest libraries/openzlib/z.lvo). This file takes
 * the place of wx's built-in zlib (libwxzlib), so no zlib is compiled in.
 *
 * The init calls take no version or size (z.library is the version);
 * z.library allocates through AllocVec itself, so zalloc/zfree are not
 * called. z_stream is zlib.h's (56 bytes on the 68k), as z.library's is.
 * The library opens on the first call and closes when the program exits.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <stdlib.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <inline/macros.h>
#include "zlib.h"

static struct Library *ZBase;

static void ZClose(void)
{
    if (ZBase)
        CloseLibrary(ZBase);
    ZBase = NULL;
}

static int ZOpen(void)
{
    if (!ZBase) {
        ZBase = OpenLibrary((CONST_STRPTR)"z.library", 0);
        if (ZBase)
            atexit(ZClose);
    }
    return ZBase != NULL;
}

#define Z_NOLIB Z_VERSION_ERROR

const char *zlibVersion(void)
{
    if (!ZOpen())
        return "0.0.0 (no z.library)";
    return LP0(30, const char *, ZlibVersion, , ZBase);
}

int deflateInit_(z_streamp s, int level, const char *version, int size)
{
    (void)version; (void)size;
    if (!ZOpen())
        return Z_NOLIB;
    return LP2(36, int, DeflateInit, z_streamp, s, a0, int, level, d0, , ZBase);
}

int deflate(z_streamp s, int flush)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP2(42, int, Deflate, z_streamp, s, a0, int, flush, d0, , ZBase);
}

int deflateEnd(z_streamp s)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP1(48, int, DeflateEnd, z_streamp, s, a0, , ZBase);
}

int inflateInit_(z_streamp s, const char *version, int size)
{
    (void)version; (void)size;
    if (!ZOpen())
        return Z_NOLIB;
    return LP1(54, int, InflateInit, z_streamp, s, a0, , ZBase);
}

int inflate(z_streamp s, int flush)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP2(60, int, Inflate, z_streamp, s, a0, int, flush, d0, , ZBase);
}

int inflateEnd(z_streamp s)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP1(66, int, InflateEnd, z_streamp, s, a0, , ZBase);
}

int deflateInit2_(z_streamp s, int level, int method, int windowBits, int memLevel, int strategy,
                  const char *version, int size)
{
    (void)version; (void)size;
    if (!ZOpen())
        return Z_NOLIB;
    return LP6(72, int, DeflateInit2, z_streamp, s, a0, int, level, d0, int, method, d1,
               int, windowBits, d2, int, memLevel, d3, int, strategy, d4, , ZBase);
}

int deflateSetDictionary(z_streamp s, const Bytef *dict, uInt len)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP3(78, int, DeflateSetDictionary, z_streamp, s, a0, const Bytef *, dict, a1, uInt, len, d0, , ZBase);
}

int deflateReset(z_streamp s)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP1(90, int, DeflateReset, z_streamp, s, a0, , ZBase);
}

int deflateParams(z_streamp s, int level, int strategy)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP3(96, int, DeflateParams, z_streamp, s, a0, int, level, d0, int, strategy, d1, , ZBase);
}

uLong deflateBound(z_streamp s, uLong sourceLen)
{
    if (!ZOpen())
        return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13;
    return LP2(108, uLong, DeflateBound, z_streamp, s, a0, uLong, sourceLen, d0, , ZBase);
}

int inflateInit2_(z_streamp s, int windowBits, const char *version, int size)
{
    (void)version; (void)size;
    if (!ZOpen())
        return Z_NOLIB;
    return LP2(132, int, InflateInit2, z_streamp, s, a0, int, windowBits, d0, , ZBase);
}

int inflateSetDictionary(z_streamp s, const Bytef *dict, uInt len)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP3(138, int, InflateSetDictionary, z_streamp, s, a0, const Bytef *, dict, a1, uInt, len, d0, , ZBase);
}

int inflateSync(z_streamp s)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP1(150, int, InflateSync, z_streamp, s, a0, , ZBase);
}

int inflateReset(z_streamp s)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP1(162, int, InflateReset, z_streamp, s, a0, , ZBase);
}

int inflateReset2(z_streamp s, int windowBits)
{
    if (!ZOpen())
        return Z_STREAM_ERROR;
    return LP2(168, int, InflateReset2, z_streamp, s, a0, int, windowBits, d0, , ZBase);
}

int compress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    if (!ZOpen())
        return Z_NOLIB;
    return LP4(198, int, Compress, Bytef *, dest, a0, uLongf *, destLen, a1, const Bytef *, source, a2,
               uLong, sourceLen, d0, , ZBase);
}

int compress2(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level)
{
    if (!ZOpen())
        return Z_NOLIB;
    return LP5(204, int, Compress2, Bytef *, dest, a0, uLongf *, destLen, a1, const Bytef *, source, a2,
               uLong, sourceLen, d0, int, level, d1, , ZBase);
}

uLong compressBound(uLong sourceLen)
{
    if (!ZOpen())
        return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13;
    return LP1(210, uLong, CompressBound, uLong, sourceLen, d0, , ZBase);
}

int uncompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen)
{
    if (!ZOpen())
        return Z_NOLIB;
    return LP4(216, int, Uncompress, Bytef *, dest, a0, uLongf *, destLen, a1, const Bytef *, source, a2,
               uLong, sourceLen, d0, , ZBase);
}

uLong adler32(uLong adler, const Bytef *buf, uInt len)
{
    if (!ZOpen())
        return 0;
    return LP3(228, uLong, Adler32, uLong, adler, d0, const Bytef *, buf, a0, uInt, len, d1, , ZBase);
}

uLong crc32(uLong crc, const Bytef *buf, uInt len)
{
    if (!ZOpen())
        return 0;
    return LP3(234, uLong, Crc32, uLong, crc, d0, const Bytef *, buf, a0, uInt, len, d1, , ZBase);
}

uLong adler32_z(uLong adler, const Bytef *buf, z_size_t len)
{
    return adler32(adler, buf, (uInt)len);
}

uLong crc32_z(uLong crc, const Bytef *buf, z_size_t len)
{
    return crc32(crc, buf, (uInt)len);
}

uLong adler32_combine(uLong a1, uLong a2, z_off_t len2)
{
    if (!ZOpen())
        return 0;
    return LP3(240, uLong, Adler32Combine, uLong, a1, d0, uLong, a2, d1, long, len2, d2, , ZBase);
}

uLong crc32_combine(uLong c1, uLong c2, z_off_t len2)
{
    if (!ZOpen())
        return 0;
    return LP3(246, uLong, Crc32Combine, uLong, c1, d0, uLong, c2, d1, long, len2, d2, , ZBase);
}

const char *zError(int err)
{
    static const char *const msgs[] = {
        "need dictionary", "stream end", "", "file error", "stream error",
        "data error", "insufficient memory", "buffer error", "incompatible version"
    };
    const int i = 2 - err;
    return i >= 0 && i < (int)(sizeof msgs / sizeof msgs[0]) ? msgs[i] : "";
}
