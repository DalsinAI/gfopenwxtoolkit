/*
 * wxtoolkit_library.h: what openwxtoolkit.library, the wx module it loads
 * (LIBS:openwxtoolkit/wx.module) and a program's link glue (wxtoolkit-glue.o
 * and wxtoolkit-tables.o, from scripts/wxtk-gen.py) say to each other.
 *
 * A program built for the library carries no wx code. Its calls into wx go
 * through a jump of six bytes each (JMP abs.l), and its references to wx's
 * data (class infos, event types, vtables, typeinfo, wxDefaultSize...) are
 * linked to stand-ins in its BSS. At start, before its own constructors,
 * the glue asks the library to load the module for it; the library points
 * every jump at the module's code and turns every reference to a stand-in
 * into one to the module's data, from the relocations in the program's own
 * file. The module calls the program's C library (malloc, stdio, C++
 * exceptions, ...) the same way, so one program has one of each.
 *
 * Symbols are matched by a 32-bit hash of their linker names (FNV-1a), so a
 * newer module serves an older program for every symbol both have.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#ifndef WXTOOLKIT_LIBRARY_H
#define WXTOOLKIT_LIBRARY_H

#include <exec/types.h>
#include <dos/dos.h>

#define WXTK_LIBNAME   "openwxtoolkit.library"
#define WXTK_MODULE    "LIBS:openwxtoolkit/wx.module"
#define WXTK_MAGIC     0x5758544BUL       /* "WXTK" */
#define WXTK_FORMAT    1

/* At the module's first code + 4 (after MOVEQ #-1,D0 / RTS). */
struct WXTKModuleHeader {
    ULONG magic, format, buildid;
    /* what the module gives: sorted by hash */
    ULONG nexp;
    const ULONG *exp_hash;
    const APTR *exp_addr;
    /* the C library calls the module makes: a JMP abs.l each, in this order */
    ULONG nimpc;
    UWORD *impc_stubs;
    const ULONG *impc_hash;
    /* the C library data it reaches: stand-ins in its BSS, by offset */
    ULONG nimpd;
    UBYTE *impd_base, *impd_end;
    const ULONG *impd_off;              /* rising */
    const ULONG *impd_hash;
    void (*init)(void);                 /* EH frames, then constructors */
    void (*exit)(void);                 /* destructors, then EH frames */
    const char *names;                  /* the imports' names, for errors: code then data, each ending in 0 */
};

/* A program's side, filled in by its glue and the tables. */
struct WXTKApp {
    ULONG magic, format, buildid;
    /* the wx calls it makes: a JMP abs.l each */
    ULONG ncode;
    UWORD *code_stubs;
    const ULONG *code_hash;
    /* the wx data it reaches: stand-ins in its BSS */
    ULONG ndata;
    UBYTE *ph_base, *ph_end;
    const ULONG *ph_off;                /* rising */
    const ULONG *ph_hash;
    /* its C library, for the module: sorted by hash */
    ULONG nrt;
    const ULONG *rt_hash;
    const APTR *rt_addr;
    /* the program as loaded, and its file */
    BPTR seglist;
    BPTR dirlock;
    CONST_STRPTR name;
    /* set by WXTK_Bind */
    BPTR module;                        /* the module's seglist */
    struct WXTKModuleHeader *header;
    ULONG fixed;                        /* references the program had turned to the module */
    char error[200];
};

/* openwxtoolkit.library's calls */
#define WXTK_LVO_BIND   (-30)           /* LONG WXTK_Bind(struct WXTKApp *app) (A0): 0 or an error, app->error says */
#define WXTK_LVO_UNBIND (-36)           /* void WXTK_Unbind(struct WXTKApp *app) (A0) */

#endif
