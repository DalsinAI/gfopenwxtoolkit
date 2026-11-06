/*
 * wxtk_glue.c: a program's start-up for openwxtoolkit.library (linked into
 * every program built with wx-config --libs ... and WXTK_SHARED=1). Before the
 * program's own constructors and EH frames (libnix's init list, priority -5,
 * linked before libnix's own entries at -5, so first among them; the C
 * library's threads start at -6) it opens the
 * library, which makes this program's copy of the wx module's data, joins
 * the two and runs the module's start (wxtoolkit_library.h); __wxtk_a4 is
 * then A4 for every call into the module. At exit, the module's atexit list
 * (the program's static destructors are on it too) and destructors run at
 * the same priority, before the libraries close.
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
    __asm volatile ("jsr -30(%%a6)" : "=r"(d0) : "r"(a6), "r"(a0) : "d1", "a1", "cc", "memory");
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

void __wxtk_exit(void)
{
    struct WXTKApp *app = &__wxtk_app;
    if (wxtk_base) {
        wxtk_unbind(app);
        CloseLibrary(wxtk_base);
        wxtk_base = NULL;
    }
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
    if (GetVar((CONST_STRPTR)"WXTK_DEBUG", (STRPTR)wxtk_progname, 4, 0) >= 0 && Output())
        Write(Output(), (APTR)"openwxtoolkit: bound\n", 21);
}
ADD2INIT(__wxtk_init, -5);
ADD2EXIT(__wxtk_exit, -5);
