/*
 * wxtk_glue.c: a program's start-up for openwxtoolkit.library (linked into
 * every program built with wx-config --libs ... and WXTK_SHARED=1). Before the
 * program's own constructors and EH frames (libnix's init list, priority -5,
 * linked before libnix's own entries at -5, so first among them; the C
 * library's threads start at -6) it opens the
 * library, which makes this program's copy of the wx module's data, joins
 * the two and runs the module's start (wxtoolkit_library.h); __wxtk_a4 is
 * then A4 for every call into the module. At exit (priority -6: after libnix's
 * entries at -5, whose EH frame end still calls into the module, and before
 * the C library's threads and libraries close) the module's atexit list (the
 * program's static destructors are on it too) and destructors run and its
 * copy goes.
 * Copyright (c) 2026 AmiCradle Team
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <stdlib.h>
#include <stabs.h>
#include "wxtoolkit_library.h"

extern struct WXTKApp __wxtk_app;
extern APTR __wxtk_a4;
extern struct WBStartup *_WBenchMsg;
static struct Library *wxtk_base;
static char wxtk_progname[256];

static LONG wxtk_bind(struct WXTKApp *app)
{
    register LONG d0 __asm("d0");
    register struct Library *a6 __asm("a6") = wxtk_base;
    register struct WXTKApp *a0 __asm("a0") = app;
    /* (a library call may change A0: it is an output too) */
    __asm volatile ("jsr -30(%%a6)" : "=r"(d0), "+r"(a0) : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}

static void wxtk_unbind(struct WXTKApp *app)
{
    register struct Library *a6 __asm("a6") = wxtk_base;
    register struct WXTKApp *a0 __asm("a0") = app;
    __asm volatile ("jsr -36(%%a6)" : "+r"(a0) : "r"(a6) : "d0", "d1", "a1", "cc", "memory");
}

static void wxtk_fail(const char *msg)
{
    BPTR out = Output();
    if (out && !_WBenchMsg) {
        Write(out, (APTR)msg, (LONG)__builtin_strlen(msg));
        Write(out, (APTR)"\n", 1);
    } else {
        struct Library *IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
        if (IntuitionBase) {
            struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"GFOpenWX Toolkit", (UBYTE *)"%s", (UBYTE *)"OK" };
            APTR args[1] = { (APTR)msg };
            EasyRequestArgs(NULL, &es, NULL, args);
            CloseLibrary(IntuitionBase);
        }
    }
    if (wxtk_base) CloseLibrary(wxtk_base);
    wxtk_base = NULL;
    exit(20);
}

static int wxtk_debug;

void __wxtk_exit(void)
{
    struct WXTKApp *app = &__wxtk_app;
    if (wxtk_base) {
        if (wxtk_debug && Output()) Write(Output(), (APTR)"openwxtoolkit: unbinding\n", 25);
        wxtk_unbind(app);
        if (wxtk_debug && Output()) Write(Output(), (APTR)"openwxtoolkit: unbound\n", 23);
        CloseLibrary(wxtk_base);
        wxtk_base = NULL;
    }
}

/* ---- the module calls the program's C library makes ----------------------------------
 * The program's own code is built -ffixed-a4 and never keeps anything in A4,
 * but its C library is not: libnix's ___init_eh holds the address it calls
 * in A4 across its calls to __register_frame_info. A module stub leaves A4
 * on the module's data, so these calls go through one that gives the caller
 * its A4 back. (They are strong: the tables' stubs of the same name are weak.) */

static ULONG wxtk_fnv(const char *s)
{
    ULONG h = 0x811C9DC5;
    while (*s) h = (h ^ (UBYTE)*s++) * 0x01000193;
    return h;
}

static APTR wxtk_target(const char *name)
{
    const struct WXTKApp *app = &__wxtk_app;
    ULONG h = wxtk_fnv(name), i;
    for (i = 0; i < app->ncode; i++)
        if (app->code_hash[i] == h) return *(APTR *)(app->code_stubs + 6 * i + 4);
    return NULL;
}

static APTR wxtk_reg_fi, wxtk_dereg_fi;

/* fn(a, b) with A4 on the module's data, and the caller's A4 kept */
static APTR wxtk_call2(APTR fn, APTR a, APTR b)
{
    register APTR r0 __asm("d0");
    register APTR r1 __asm("d1") = a;
    register APTR ra0 __asm("a0") = fn;
    register APTR ra1 __asm("a1") = b;
    __asm volatile ("move.l %%a4,-(%%sp)\n\t"
                    "move.l %3,-(%%sp)\n\t"
                    "move.l %1,-(%%sp)\n\t"
                    "movea.l ___wxtk_a4,%%a4\n\t"
                    "jsr (%2)\n\t"
                    "addq.l #8,%%sp\n\t"
                    "movea.l (%%sp)+,%%a4"
                    : "=r"(r0), "+r"(r1), "+r"(ra0), "+r"(ra1) : : "cc", "memory");
    return r0;
}


static void wxtk_say(const char *what, APTR a, APTR b)
{
    static const char hex[] = "0123456789abcdef";
    char buf[80], *p = buf;
    ULONG v[2], k, i;
    BPTR out = Output();
    if (!wxtk_debug || !out) return;
    while (*what) *p++ = *what++;
    v[0] = (ULONG)a; v[1] = (ULONG)b;
    for (k = 0; k < 2; k++) {
        *p++ = ' '; *p++ = '$';
        for (i = 0; i < 8; i++) *p++ = hex[(v[k] >> (28 - 4 * i)) & 15];
    }
    *p++ = '\n';
    Write(out, buf, p - buf);
}

void __register_frame_info(const void *begin, void *ob)
{
    wxtk_say("openwxtoolkit: program EH frames", (APTR)begin, wxtk_reg_fi);
    wxtk_say("openwxtoolkit: A4 for the module, stub 0", __wxtk_a4, *(APTR *)(__wxtk_app.code_stubs + 1));
    wxtk_say("openwxtoolkit: where A4 is kept, stub 0's", &__wxtk_a4, *(APTR *)(__wxtk_app.code_stubs + 1));
    if (wxtk_reg_fi) wxtk_call2(wxtk_reg_fi, (APTR)begin, ob);
    wxtk_say("openwxtoolkit: registered", (APTR)begin, ob);
}

void *__deregister_frame_info(const void *begin)
{
    return wxtk_dereg_fi ? wxtk_call2(wxtk_dereg_fi, (APTR)begin, NULL) : NULL;
}

void __wxtk_init(void)
{
    struct WXTKApp *app = &__wxtk_app;
    struct Process *pr = (struct Process *)FindTask(NULL);
    if (_WBenchMsg) {
        struct WBArg *wa = _WBenchMsg->sm_ArgList;
        app->dirlock = wa[0].wa_Lock;
        app->name = (CONST_STRPTR)wa[0].wa_Name;
        app->seglist = ((BPTR *)BADDR(pr->pr_SegList))[3];
    } else {
        struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
        if (!cli || !GetProgramName((STRPTR)wxtk_progname, sizeof wxtk_progname)) wxtk_fail("openwxtoolkit: can't tell which program this is");
        app->seglist = cli->cli_Module;
        app->name = FilePart((CONST_STRPTR)wxtk_progname);
        app->dirlock = GetProgramDir();
    }
    wxtk_base = OpenLibrary((CONST_STRPTR)WXTK_LIBNAME, 1);
    if (!wxtk_base) wxtk_fail("This program needs " WXTK_LIBNAME " (GFOpenWX Toolkit) in LIBS:");
    if (wxtk_bind(app)) wxtk_fail(app->error);
    __wxtk_a4 = app->a4;
    wxtk_reg_fi = wxtk_target("___register_frame_info");
    wxtk_dereg_fi = wxtk_target("___deregister_frame_info");
    {
        char v[4];
        wxtk_debug = GetVar((CONST_STRPTR)"WXTK_DEBUG", (STRPTR)v, sizeof v, 0) >= 0;
        /* (a test: the program's own exception tables left out) */
        if (GetVar((CONST_STRPTR)"WXTK_NOEH", (STRPTR)v, sizeof v, 0) >= 0) wxtk_reg_fi = wxtk_dereg_fi = NULL;
    }
    wxtk_say("openwxtoolkit: bound, A4", __wxtk_a4, app->a4);
}
ADD2INIT(__wxtk_init, -5);
ADD2EXIT(__wxtk_exit, -6);
