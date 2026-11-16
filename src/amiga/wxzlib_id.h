/*
 * Which jump table a z.library has, from its id string and version.
 * GFOpenZlib before the rename said "OpenZlib, AmigaChrome Team" (53.12,
 * still on the instances), so the name is matched without the "GF".
 * Plain C, no Amiga headers: tests/host/zlib_id_test.c runs it on the host.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#ifndef WXZLIB_ID_H
#define WXZLIB_ID_H

#include <string.h>

enum
{
    Z_TABLE_AMINET = 0, /* the Aminet z.library's table (z.lvo) only */
    Z_TABLE_OLD    = 1, /* GFOpenZlib before 53.13: its own old table */
    Z_TABLE_OWN    = 2  /* GFOpenZlib 53.13 on: z.lvo plus its own calls past -270 */
};

static inline int wxzlib_table( const char* id, int version, int revision )
{
    int ours = id && strstr( id, "OpenZlib" ) != NULL;

    if( !ours )
        return Z_TABLE_AMINET;

    if( version < 53 || ( version == 53 && revision < 13 ) )
        return Z_TABLE_OLD;

    return Z_TABLE_OWN;
}

#endif
