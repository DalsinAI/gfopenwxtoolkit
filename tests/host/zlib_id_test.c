/*
 * Host test: which z.library jump table the zlib glue picks
 * (src/amiga/wxzlib_id.h). cc -I src/amiga tests/host/zlib_id_test.c
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <stdio.h>
#include "wxzlib_id.h"

static int fails;

static void check( const char* id, int ver, int rev, int want )
{
    int got = wxzlib_table( id, ver, rev );
    printf( "%s %-44s %d.%d -> %d\n", got == want ? "ok  " : "FAIL", id ? id : "(null)", ver, rev,
            got );
    if( got != want )
        fails++;
}

int main( void )
{
    /* the instances' z.library before the rename */
    check( "z.library 53.12 (13/10/26) OpenZlib, AmigaChrome Team\r\n", 53, 12, Z_TABLE_OLD );
    check( "z.library 53.12 GFOpenZlib, AmiCradle Team\r\n", 53, 12, Z_TABLE_OLD );
    check( "z.library 52.1 OpenZlib\r\n", 52, 1, Z_TABLE_OLD );
    check( "z.library 53.13 (07.11.2026) GFOpenZlib, AmiCradle Team\r\n", 53, 13, Z_TABLE_OWN );
    check( "z.library 53.15 (10.11.2026) GFOpenZlib, AmiCradle Team\r\n", 53, 15, Z_TABLE_OWN );
    check( "z.library 54.0 GFOpenZlib\r\n", 54, 0, Z_TABLE_OWN );
    /* Aminet z.library and anything else: z.lvo only */
    check( "z.library 1.6 (12.04.2017)\r\n", 1, 6, Z_TABLE_AMINET );
    check( "z.library 53.12 zlib 1.2.11\r\n", 53, 12, Z_TABLE_AMINET );
    check( NULL, 53, 12, Z_TABLE_AMINET );
    printf( "%s\n", fails ? "FAILED" : "all ok" );
    return fails != 0;
}
