/*
 * wxtoolkit_library.h: what openwxtoolkit.library, the wx module it loads
 * (LIBS:openwxtoolkit/wx.module) and a program's link glue (wxtoolkit-glue.o
 * and wxtoolkit-tables.o, from scripts/wxtk-gen.py) say to each other.
 *
 * Shared code. wx (and the C++ runtime it is built with: libstdc++,
 * libsupc++ and the unwinder) is compiled -fbaserel32 and linked
 * -resident32 into one module: its code reaches every global through A4.
 * The library loads the module once; each program that binds gets its own
 * copy of the module's data, and runs with A4 on it.
 *
 * A program built for the library carries no wx and no C++ runtime. It is
 * compiled -ffixed-a4 (wx-config --cxxflags, WXTK_SHARED=1), so its own code
 * never touches A4. Its calls into the module go through a stub of twelve
 * bytes each, which sets A4 from the program's __wxtk_a4 and jumps on
 * (MOVEA.L abs.l,A4 / JMP abs.l): so a call from any of the program's tasks,
 * or from code of its own that isn't -ffixed-a4, reaches the module with the
 * right A4. Its references to the module's data (class infos, event types,
 * wxDefaultSize, std::cout...) are linked to stand-ins in its BSS; binding
 * turns every one of them, from the relocations in the program's own file,
 * to the program's copy (data that changes) or to the module's data as
 * loaded (vtables, typeinfo and other constants, which the module's code
 * reaches there itself).
 *
 * The other way, the module calls the program's C library (libnix: malloc,
 * stdio, maths, sockets, pthreads, amiga.lib's calls) through a slot each in
 * its data, which binding fills in, and starts with a copy of the program's
 * values of a few of its variables (the library bases, errno's and stdio's
 * pointers). Callbacks from the program's C library into the module get A4
 * set on the way: the module has its own qsort, bsearch, atexit and
 * __cxa_atexit, and wraps pthread_create, pthread_once and
 * pthread_key_create (wxtk_module_rt.c).
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
#define WXTK_FORMAT    2
#define WXTK_A4_OFFSET 0x7ffe             /* A4 = data + 0x7ffe (libnix's -resident32 layout) */

/* At the module's first code + 4 (after a BRA.W over it). */
struct WXTKModuleHeader {
    ULONG magic, format, buildid;
    LONG (*entry)(void);                /* with A4 on a copy: EH frames, constructors; 0 when it worked */
    void (*exit)(void);                 /* with A4 on the copy: atexit list, destructors, EH frames */
    UBYTE *a4_init;                     /* A4 for the data as loaded: its start + WXTK_A4_OFFSET */
    ULONG data_size;                    /* data and BSS together, bytes: each program's copy */
    const ULONG *relocs;                /* data-to-data relocations: a count, then offsets into the data */
    /* what the module gives, sorted by hash: an address as loaded each, and
     * whether it is data a program has its own copy of */
    ULONG nexp;
    const ULONG *exp_hash;
    const APTR *exp_addr;
    const UBYTE *exp_own;
    /* the C library calls the module makes: a slot each, in its data */
    ULONG nimpc;
    const ULONG *impc_hash;
    APTR *impc_slot;                    /* as loaded: a program's copy has it at the same offset */
    /* the C library variables it starts with a copy of */
    ULONG nimpd;
    const ULONG *impd_hash;
    UBYTE *const *impd_place;           /* as loaded */
    const ULONG *impd_size;
    const char *names;                  /* the imports' names, for errors: calls then variables, each ending in 0 */
};

/* A program's side, filled in by its glue and the tables. */
struct WXTKApp {
    ULONG magic, format, buildid;
    /* the module calls it makes: a stub each (MOVEA.L abs.l,A4 / JMP abs.l, twelve bytes) */
    ULONG ncode;
    UWORD *code_stubs;
    const ULONG *code_hash;
    /* the module data it reaches: stand-ins in its BSS */
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
    APTR a4;                            /* A4 on the program's copy of the module's data */
    UBYTE *copy;
    struct WXTKModuleHeader *header;
    ULONG fixed;                        /* references the program had turned to the module */
    char error[200];
};

/* openwxtoolkit.library's calls */
#define WXTK_LVO_BIND   (-30)           /* LONG WXTK_Bind(struct WXTKApp *app) (A0): 0 or an error, app->error says */
#define WXTK_LVO_UNBIND (-36)           /* void WXTK_Unbind(struct WXTKApp *app) (A0): the module's exit, then its copy freed */

#endif
