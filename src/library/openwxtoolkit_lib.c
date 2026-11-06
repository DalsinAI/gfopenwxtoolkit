/*
 * openwxtoolkit.library: wxWidgets (GFOpenWX Toolkit) for programs that
 * don't carry it. wxtoolkit_library.h says how a program, the library and
 * the wx module fit together.
 *
 * WXTK_Bind loads LIBS:openwxtoolkit/wx.module for the calling program (a
 * copy each: step 1), relocates it, points its C library calls and data at
 * the program's, and then turns the program's own jumps and data references
 * to the module. Everything runs on the caller's task; the library keeps no
 * state of its own past dos.library, so any number of programs bind at once.
 *
 * Built -nostdlib: exec and dos only, through the base.
 * Copyright (c) 2026 AmiCradle Team
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/doshunks.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "wxtoolkit_library.h"

#define REG(r, decl) register decl __asm(#r)
#define LIB_VERSION 1
#define LIB_REVISION 0

struct WXTKBase {
    struct Library lib;
    BPTR seglist;
    struct ExecBase *sys;
    struct DosLibrary *dos;
};

#define EXEC(b) struct ExecBase *SysBase = (b)->sys
#define DOS(b) struct DosLibrary *DOSBase = (b)->dos

/* Run as a program, the library does nothing (the first code: built with
 * -fno-toplevel-reorder). */
int start(void) { return -1; }

static const char lib_name[] = WXTK_LIBNAME;
static const char lib_id[] = "openwxtoolkit.library 1.0 (6.11.2026) GFOpenWX Toolkit, AmiCradle Team\r\n";
static const char lib_ver[] __attribute__((used)) = "\0$VER: openwxtoolkit.library 1.0 (6.11.2026) GFOpenWX Toolkit, AmiCradle Team";

static struct Library *lib_init(REG(d0, struct WXTKBase *base), REG(a0, BPTR seglist), REG(a6, struct ExecBase *sys));
static struct Library *lib_open(REG(a6, struct WXTKBase *base));
static BPTR lib_close(REG(a6, struct WXTKBase *base));
static BPTR lib_expunge(REG(a6, struct WXTKBase *base));
static ULONG lib_null(void);
static LONG WXTK_Bind(REG(a0, struct WXTKApp *app), REG(a6, struct WXTKBase *base));
static void WXTK_Unbind(REG(a0, struct WXTKApp *app), REG(a6, struct WXTKBase *base));

static const APTR lib_vectors[] = {
    (APTR)lib_open, (APTR)lib_close, (APTR)lib_expunge, (APTR)lib_null,
    (APTR)WXTK_Bind, (APTR)WXTK_Unbind,
    (APTR)-1,
};
static const struct { ULONG size; const APTR *vectors; APTR data; APTR init; } lib_inittable = {
    sizeof(struct WXTKBase), lib_vectors, NULL, (APTR)lib_init,
};
const struct Resident lib_romtag = {
    RTC_MATCHWORD, (struct Resident *)&lib_romtag, (APTR)(&lib_romtag + 1),
    RTF_AUTOINIT, LIB_VERSION, NT_LIBRARY, 0, (char *)lib_name, (char *)lib_id, (APTR)&lib_inittable,
};

static struct Library *lib_init(REG(d0, struct WXTKBase *base), REG(a0, BPTR seglist), REG(a6, struct ExecBase *sys))
{
    struct ExecBase *SysBase = sys;
    base->sys = sys;
    base->seglist = seglist;
    base->lib.lib_Revision = LIB_REVISION;
    base->dos = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 37);
    if (!base->dos) {
        FreeMem((UBYTE *)base - base->lib.lib_NegSize, base->lib.lib_NegSize + base->lib.lib_PosSize);
        return NULL;
    }
    return &base->lib;
}
static struct Library *lib_open(REG(a6, struct WXTKBase *base))
{
    base->lib.lib_OpenCnt++;
    base->lib.lib_Flags &= ~LIBF_DELEXP;
    return &base->lib;
}
static BPTR lib_close(REG(a6, struct WXTKBase *base))
{
    base->lib.lib_OpenCnt--;
    if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP)) return lib_expunge(base);
    return 0;
}
static BPTR lib_expunge(REG(a6, struct WXTKBase *base))
{
    EXEC(base);
    BPTR seglist;
    if (base->lib.lib_OpenCnt) { base->lib.lib_Flags |= LIBF_DELEXP; return 0; }
    seglist = base->seglist;
    CloseLibrary((struct Library *)base->dos);
    Remove(&base->lib.lib_Node);
    FreeMem((UBYTE *)base - base->lib.lib_NegSize, base->lib.lib_NegSize + base->lib.lib_PosSize);
    return seglist;
}
static ULONG lib_null(void) { return 0; }

/* ---- small helpers (no C library here) -------------------------------------------- */

static void say(struct WXTKApp *app, const char *a, const char *b, ULONG n, int hex)
{
    char *d = app->error, *e = app->error + sizeof app->error - 1;
    while (*a && d < e) *d++ = *a++;
    if (b) while (*b && d < e) *d++ = *b++;
    if (hex >= 0) {
        char t[12]; int k = 0;
        if (hex) { int s; for (s = 28; s >= 0; s -= 4) t[k++] = "0123456789abcdef"[(n >> s) & 15]; }
        else { char r[12]; int j = 0; do { r[j++] = (char)('0' + n % 10); n /= 10; } while (n); while (j) t[k++] = r[--j]; }
        if (d < e && hex) *d++ = '$';
        for (int i = 0; i < k && d < e; i++) *d++ = t[i];
    }
    *d = 0;
}

/* index of h in a rising table, or -1 */
static LONG find(const ULONG *tab, ULONG n, ULONG h)
{
    ULONG lo = 0, hi = n;
    while (lo < hi) {
        ULONG mid = (lo + hi) >> 1;
        if (tab[mid] < h) lo = mid + 1; else hi = mid;
    }
    return (lo < n && tab[lo] == h) ? (LONG)lo : -1;
}

/* the stand-in a value points into: the last offset not past it */
static LONG which(const ULONG *off, ULONG n, ULONG rel)
{
    ULONG lo = 0, hi = n;
    while (lo < hi) {
        ULONG mid = (lo + hi) >> 1;
        if (off[mid] <= rel) lo = mid + 1; else hi = mid;
    }
    return (LONG)lo - 1;
}

/* ---- a hunk file, read through a buffer ------------------------------------------- */

struct rd {
    struct ExecBase *sys;
    struct DosLibrary *dos;
    BPTR fh;
    UBYTE *buf;
    LONG len, pos;
    int bad;
};
#define RDBUF 16384

static int rd_fill(struct rd *r)
{
    struct DosLibrary *DOSBase = r->dos;
    r->len = Read(r->fh, r->buf, RDBUF);
    r->pos = 0;
    if (r->len <= 0) { r->len = 0; r->bad = 1; return 0; }
    return 1;
}
static ULONG rd_long(struct rd *r)
{
    ULONG v = 0;
    for (int i = 0; i < 4; i++) {
        if (r->pos >= r->len && !rd_fill(r)) return 0;
        v = (v << 8) | r->buf[r->pos++];
    }
    return v;
}
static UWORD rd_word(struct rd *r)
{
    UWORD v = 0;
    for (int i = 0; i < 2; i++) {
        if (r->pos >= r->len && !rd_fill(r)) return 0;
        v = (UWORD)((v << 8) | r->buf[r->pos++]);
    }
    return v;
}
/* n bytes into dst (NULL: skipped) */
static void rd_bytes(struct rd *r, UBYTE *dst, ULONG n)
{
    struct ExecBase *SysBase = r->sys;
    struct DosLibrary *DOSBase = r->dos;
    while (n && !r->bad) {
        if (r->pos < r->len) {
            ULONG k = (ULONG)(r->len - r->pos);
            if (k > n) k = n;
            if (dst) { CopyMem(r->buf + r->pos, dst, k); dst += k; }
            r->pos += (LONG)k; n -= k;
        } else if (n >= RDBUF) {
            /* big runs straight from the file */
            if (dst) {
                LONG got = Read(r->fh, dst, (LONG)(n & ~(ULONG)3));
                if (got <= 0) { r->bad = 1; return; }
                dst += got; n -= (ULONG)got;
            } else {
                if (Seek(r->fh, (LONG)n, OFFSET_CURRENT) < 0) { r->bad = 1; return; }
                n = 0;
            }
        } else if (!rd_fill(r)) return;
    }
}
static void rd_rewind(struct rd *r)
{
    struct DosLibrary *DOSBase = r->dos;
    Seek(r->fh, 0, OFFSET_BEGINNING);
    r->len = r->pos = 0;
    r->bad = 0;
}

/* What a walk does with each relocation: pass 1 of a load adds the target
 * hunk's base; a fix pass turns values inside the stand-ins elsewhere. */
struct walk {
    struct WXTKApp *app;
    UBYTE **base;                       /* each hunk's memory */
    ULONG *size;                        /* and its size in bytes */
    ULONG nh;
    int load;                           /* 1: allocate and read the hunks, relocate */
    /* fix: values in [lo, hi) are the stand-ins */
    UBYTE *lo, *hi;
    const ULONG *off;
    ULONG noff;
    const APTR *to;                     /* each stand-in's new address */
    ULONG fixed;
};

static int fix_one(struct walk *w, ULONG *p)
{
    UBYTE *v = (UBYTE *)*p;
    if (v >= w->lo && v < w->hi) {
        ULONG rel = (ULONG)(v - w->lo);
        LONG i = which(w->off, w->noff, rel);
        if (i < 0) return 1;
        if (!w->to[i]) {
            say(w->app, "openwxtoolkit: the wx module lacks data the program uses (hash) ", 0, w->app->ph_hash[i], 1);
            return 0;
        }
        *p = (ULONG)w->to[i] + (rel - w->off[i]);
        w->fixed++;
    }
    return 1;
}

static int reloc(struct walk *w, ULONG h, ULONG target, ULONG off)
{
    ULONG *p;
    if (h >= w->nh || target >= w->nh || off + 4 > w->size[h]) {
        say(w->app, "openwxtoolkit: a relocation outside its hunk", 0, 0, -1);
        return 0;
    }
    p = (ULONG *)(w->base[h] + off);
    if (w->load) { *p += (ULONG)w->base[target]; return 1; }
    return fix_one(w, p);
}

/* Walks a hunk file: HUNK_HEADER, then each hunk's contents and relocations.
 * Loading, it allocates each hunk as LoadSeg does (UnLoadSeg frees them). */
static int walk_file(struct rd *r, struct walk *w, struct ExecBase *SysBase)
{
    ULONG t, n, first, last, h = 0, i;
    if (rd_long(r) != HUNK_HEADER) { say(w->app, "openwxtoolkit: not a program: ", (const char *)w->app->name, 0, -1); return 0; }
    while ((n = rd_long(r)) != 0 && !r->bad) rd_bytes(r, NULL, n * 4);  /* resident library names */
    n = rd_long(r); first = rd_long(r); last = rd_long(r);
    if (r->bad || first != 0 || last + 1 != n || n == 0) { say(w->app, "openwxtoolkit: a hunk header it can't read", 0, 0, -1); return 0; }
    if (w->load) {
        w->nh = n;
        w->base = AllocVec(n * (sizeof(UBYTE *) + sizeof(ULONG)), MEMF_ANY | MEMF_CLEAR);
        if (!w->base) { say(w->app, "openwxtoolkit: out of memory", 0, 0, -1); return 0; }
        w->size = (ULONG *)(w->base + n);
    } else if (n != w->nh) {
        say(w->app, "openwxtoolkit: the program file doesn't match the program running: ", (const char *)w->app->name, 0, -1);
        return 0;
    }
    for (i = 0; i < n; i++) {
        ULONG s = rd_long(r), flags = MEMF_ANY | MEMF_CLEAR;
        if ((s & 0xC0000000UL) == 0xC0000000UL) flags = rd_long(r) | MEMF_CLEAR;
        else if (s & 0x40000000UL) flags |= MEMF_CHIP;
        s = (s & 0x3FFFFFFFUL) * 4;
        if (w->load) {
            ULONG *m = AllocMem(s + 8, flags);
            if (!m) { say(w->app, "openwxtoolkit: out of memory for the wx module (bytes) ", 0, s, 0); return 0; }
            m[0] = s + 8;
            m[1] = 0;
            if (i) ((ULONG *)w->base[i - 1])[-1] = MKBADDR(&m[1]);
            w->base[i] = (UBYTE *)&m[2];
            w->size[i] = s;
        } else if (s > w->size[i]) {
            say(w->app, "openwxtoolkit: the program file doesn't match the program running: ", (const char *)w->app->name, 0, -1);
            return 0;
        }
    }
    while (!r->bad) {
        t = rd_long(r);
        if (r->bad) break;
        switch (t & 0x3FFFFFFFUL) {
        case HUNK_CODE: case HUNK_DATA:
            n = rd_long(r) * 4;
            if (h >= w->nh || n > w->size[h]) { say(w->app, "openwxtoolkit: a hunk bigger than its header says", 0, 0, -1); return 0; }
            rd_bytes(r, w->load ? w->base[h] : NULL, n);
            break;
        case HUNK_BSS:
            rd_long(r);
            break;
        case HUNK_RELOC32:
            while ((n = rd_long(r)) != 0 && !r->bad) {
                ULONG target = rd_long(r);
                while (n--) if (!reloc(w, h, target, rd_long(r))) return 0;
            }
            break;
        case HUNK_DREL32: case 0x3FC /* HUNK_RELOC32SHORT */: {
            ULONG words = 0;
            while ((n = rd_word(r)) != 0 && !r->bad) {
                ULONG target = rd_word(r);
                words += 2 + n;
                while (n--) if (!reloc(w, h, target, rd_word(r))) return 0;
            }
            words++;
            if (words & 1) rd_word(r);
            break;
        }
        case HUNK_SYMBOL:
            while ((n = rd_long(r)) != 0 && !r->bad) rd_bytes(r, NULL, (n & 0xFFFFFF) * 4 + 4);
            break;
        case HUNK_DEBUG:
            n = rd_long(r);
            rd_bytes(r, NULL, n * 4);
            break;
        case HUNK_END:
            if (++h == w->nh) return 1;
            break;
        default:
            say(w->app, "openwxtoolkit: a hunk type it doesn't know: ", 0, t, 1);
            return 0;
        }
    }
    say(w->app, "openwxtoolkit: the file ends early: ", (const char *)w->app->name, 0, -1);
    return 0;
}

static BPTR open_rel(struct DosLibrary *DOSBase, BPTR dir, CONST_STRPTR name)
{
    BPTR old, fh;
    if (!dir) return Open(name, MODE_OLDFILE);
    old = CurrentDir(dir);
    fh = Open(name, MODE_OLDFILE);
    CurrentDir(old);
    return fh;
}

/* ---- binding --------------------------------------------------------------------- */

static LONG WXTK_Bind(REG(a0, struct WXTKApp *app), REG(a6, struct WXTKBase *base))
{
    EXEC(base);
    DOS(base);
    struct rd r;
    struct walk w;
    struct WXTKModuleHeader *mh;
    APTR *to = NULL;
    UBYTE *buf = NULL;
    BPTR fh = 0;
    ULONG i, nrt;
    LONG rc = 20;

    app->error[0] = 0;
    app->module = 0;
    app->fixed = 0;
    if (app->magic != WXTK_MAGIC || app->format != WXTK_FORMAT) { say(app, "openwxtoolkit: a program for another version of the library", 0, 0, -1); return 20; }
    buf = AllocVec(RDBUF, MEMF_ANY);
    if (!buf) { say(app, "openwxtoolkit: out of memory", 0, 0, -1); return 20; }

    /* 1. the module, a copy for this program */
    fh = Open((CONST_STRPTR)WXTK_MODULE, MODE_OLDFILE);
    if (!fh) { say(app, "openwxtoolkit: can't open " WXTK_MODULE, 0, 0, -1); goto out; }
    r.sys = base->sys; r.dos = base->dos; r.fh = fh; r.buf = buf; r.len = r.pos = 0; r.bad = 0;
    w.app = app; w.base = NULL; w.size = NULL; w.nh = 0; w.load = 1; w.fixed = 0;
    {
        CONST_STRPTR keep = app->name;
        app->name = (CONST_STRPTR)WXTK_MODULE;
        i = (ULONG)walk_file(&r, &w, SysBase);
        app->name = keep;
    }
    if (w.base && w.nh) app->module = MKBADDR((ULONG *)w.base[0] - 1);
    if (!i) goto out;
    mh = (struct WXTKModuleHeader *)(w.base[0] + 4);
    if (mh->magic != WXTK_MAGIC || mh->format != WXTK_FORMAT) { say(app, "openwxtoolkit: " WXTK_MODULE " is not a wx module of this version", 0, 0, -1); goto out; }
    app->header = mh;

    /* 2. the module's C library calls and data: the program's */
    nrt = mh->nimpc > mh->nimpd ? mh->nimpc : mh->nimpd;
    to = AllocVec((nrt + app->ndata + 1) * sizeof(APTR), MEMF_ANY);
    if (!to) { say(app, "openwxtoolkit: out of memory", 0, 0, -1); goto out; }
    {
        const char *nm = mh->names;
        for (i = 0; i < mh->nimpc; i++) {
            LONG k = find(app->rt_hash, app->nrt, mh->impc_hash[i]);
            if (k < 0) { say(app, "openwxtoolkit: the program lacks a C library call the wx module needs: ", nm, 0, -1); goto out; }
            *(APTR *)(mh->impc_stubs + 3 * i + 1) = app->rt_addr[k];
            while (*nm++) {}
        }
        for (i = 0; i < mh->nimpd; i++) {
            LONG k = find(app->rt_hash, app->nrt, mh->impd_hash[i]);
            if (k < 0) { say(app, "openwxtoolkit: the program lacks C library data the wx module needs: ", nm, 0, -1); goto out; }
            to[i] = app->rt_addr[k];
            while (*nm++) {}
        }
    }
    rd_rewind(&r);
    w.load = 0; w.lo = mh->impd_base; w.hi = mh->impd_end; w.off = mh->impd_off; w.noff = mh->nimpd; w.to = (const APTR *)to;
    if (mh->nimpd && !walk_file(&r, &w, SysBase)) goto out;
    Close(fh); fh = 0;

    /* 3. the program's wx calls and data: the module's */
    for (i = 0; i < app->ncode; i++) {
        LONG k = find(mh->exp_hash, mh->nexp, app->code_hash[i]);
        if (k < 0) { say(app, "openwxtoolkit: the wx module lacks a call the program makes (hash) ", 0, app->code_hash[i], 1); goto out; }
        *(APTR *)(app->code_stubs + 3 * i + 1) = mh->exp_addr[k];
    }
    for (i = 0; i < app->ndata; i++) {
        LONG k = find(mh->exp_hash, mh->nexp, app->ph_hash[i]);
        to[i] = k < 0 ? NULL : mh->exp_addr[k];   /* a missing one is an error only where the program uses it */
    }
    if (app->ndata) {
        struct walk p;
        ULONG nh = 0;
        BPTR s;
        for (s = app->seglist; s; s = *(BPTR *)BADDR(s)) nh++;
        p.app = app; p.nh = nh; p.load = 0; p.fixed = 0;
        p.lo = app->ph_base; p.hi = app->ph_end; p.off = app->ph_off; p.noff = app->ndata; p.to = (const APTR *)to;
        p.base = AllocVec(nh * (sizeof(UBYTE *) + sizeof(ULONG)) + 4, MEMF_ANY);
        if (!p.base) { say(app, "openwxtoolkit: out of memory", 0, 0, -1); goto out; }
        p.size = (ULONG *)(p.base + nh);
        for (i = 0, s = app->seglist; s; s = *(BPTR *)BADDR(s), i++) {
            ULONG *seg = BADDR(s);
            p.base[i] = (UBYTE *)(seg + 1);
            p.size[i] = seg[-1] - 8;
        }
        fh = open_rel(base->dos, app->dirlock, app->name);
        if (!fh) { FreeVec(p.base); say(app, "openwxtoolkit: can't read the program's own file: ", (const char *)app->name, 0, -1); goto out; }
        r.fh = fh; r.len = r.pos = 0; r.bad = 0;
        i = (ULONG)walk_file(&r, &p, SysBase);
        FreeVec(p.base);
        if (!i) goto out;
        app->fixed = p.fixed;
    }
    CacheClearU();
    rc = 0;
out:
    if (fh) Close(fh);
    if (to) FreeVec(to);
    if (w.base) FreeVec(w.base);
    FreeVec(buf);
    if (rc && app->module) { UnLoadSeg(app->module); app->module = 0; }
    return rc;
}

static void WXTK_Unbind(REG(a0, struct WXTKApp *app), REG(a6, struct WXTKBase *base))
{
    DOS(base);
    if (app->module) UnLoadSeg(app->module);
    app->module = 0;
    app->header = NULL;
}
