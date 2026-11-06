/*
 * wxtk_module_rt.c: the wx module's own start and end, and the few C
 * library calls it keeps for itself, linked into LIBS:openwxtoolkit/wx.module.
 * Built -fbaserel32 like the rest of the module: every global here is the
 * binding program's own copy (wxtoolkit_library.h).
 *
 * Start (wxtk_module_entry, with A4 on a new copy): register the module's EH
 * frames with the module's unwinder (the program's frames join it, as the
 * program's __register_frame_info is the module's), run libnix's init list
 * of the module's members, then the C++ constructors, last to first.
 * End (wxtk_module_exit): the atexit list (the program's static destructors
 * are on it too), the destructors, the exit list, the frames.
 *
 * The module calls the program's C library through slots (mod_tables.s), so
 * the program's code calls the module back now and then; A4 is whatever the
 * program's code left in it then. So the calls that take a function to call
 * later are the module's own here: qsort, bsearch, atexit, __cxa_atexit; and
 * pthread_create, pthread_once and pthread_key_create (linked --wrap) hand
 * the program's libpthread a small piece of code that sets A4 first.
 * Copyright (c) 2026 AmiCradle Team
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef void (*wxtk_fn)(void);
/* the list heads (mod_tables.s): the linker puts the lists with the code,
 * so they are reached directly, not through A4 */
extern const wxtk_fn __CTOR_LIST__[], __DTOR_LIST__[];
extern const long __INIT_LIST__[], __EXIT_LIST__[];
extern const long _EH_FRAME_BEGINS__[];
/* in the data (each program's copy has its own objects): not const, so reached through A4 */
extern long _EH_FRAME_OBJECTS__[];
extern void __register_frame_info(const void *begin, void *object);
extern void *__deregister_frame_info(const void *begin);

void *__dso_handle = &__dso_handle;

static APTR my_a4(void)
{
    APTR a4;
    __asm__ __volatile__("move.l %%a4,%0" : "=d"(a4));
    return a4;
}

/* ---- libnix's set lists, as its startup runs them ------------------------------- */

static unsigned long level;

static void callfuncs(const long *list, unsigned long dir)
{
    for (;;) {
        unsigned long next = 0, cur = level ^ dir, pri;
        const long *p = list + 1;
        void (*fn)(void);
        while ((fn = (void (*)(void))*p++) != NULL) {
            pri = (unsigned long)*p++;
            if (pri == level) fn();
            pri ^= dir;
            if (pri < cur && pri > next) next = pri;
        }
        next ^= dir;
        level = next;
        if (next == dir) break;
    }
}

/* ---- atexit ---------------------------------------------------------------------- */

struct wxtk_atexit {
    struct wxtk_atexit *next;
    void (*fn)(void *);
    void *arg;
    int plain;                              /* atexit: fn takes nothing */
};
static struct wxtk_atexit *atexits;

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso)
{
    struct wxtk_atexit *e = malloc(sizeof *e);
    (void)dso;
    if (!e) return -1;
    e->fn = fn; e->arg = arg; e->plain = 0;
    e->next = atexits;
    atexits = e;
    return 0;
}

int atexit(void (*fn)(void))
{
    int r = __cxa_atexit((void (*)(void *))fn, NULL, NULL);
    if (!r) atexits->plain = 1;
    return r;
}

static void run_atexits(void)
{
    struct wxtk_atexit *e;
    while ((e = atexits) != NULL) {
        atexits = e->next;
        if (e->plain) ((void (*)(void))e->fn)();
        else e->fn(e->arg);
        free(e);
    }
}

/* ---- start and end ----------------------------------------------------------------- */

static int started, tracing;

extern long write(int fd, const void *buf, unsigned long n);
/* with WXTK_DEBUG set, the module's start says how far it got */
static void trace(const char *what, unsigned long n)
{
    char t[64], *d = t;
    int s;
    if (!tracing) return;
    for (const char *a = "wx.module: "; *a; ) *d++ = *a++;
    while (*what && d < t + 48) *d++ = *what++;
    *d++ = ' ';
    for (s = 28; s >= 0; s -= 4) *d++ = "0123456789abcdef"[(n >> s) & 15];
    *d++ = '\n';
    write(1, t, (unsigned long)(d - t));
}

long wxtk_module_entry(void)
{
    const long *b = _EH_FRAME_BEGINS__ + 1;
    long *o = _EH_FRAME_OBJECTS__ + 1;
    const wxtk_fn *p0 = __CTOR_LIST__ + 1, *p;
    tracing = 1;    /* TEMPORARY: always, while the start is proved */
    trace("start", 0);
    tracing = getenv("WXTK_DEBUG") != NULL || 1;
    trace("start, A4", (unsigned long)my_a4());
    while (*b) __register_frame_info((const void *)*b++, (void *)*o++);
    trace("EH frames", (unsigned long)(b - _EH_FRAME_BEGINS__ - 1));
    level = 0;
    callfuncs(__INIT_LIST__, ~0UL);
    trace("init list run", 0);
    for (p = p0; *p; p++) {
    }
    while (p > p0) { --p; trace("constructor", (unsigned long)*p); (*p)(); }
    trace("started", 0);
    started = 1;
    return 0;
}

void wxtk_module_exit(void)
{
    const wxtk_fn *p = __DTOR_LIST__ + 1;
    const long *b = _EH_FRAME_BEGINS__ + 1;
    if (!started) return;
    started = 0;
    run_atexits();
    while (*p) (*p++)();
    callfuncs(__EXIT_LIST__, 0);
    while (*b) __deregister_frame_info((const void *)*b++);
}

/* ---- the C library calls that call back --------------------------------------------- */

static void swap_bytes(char *a, char *b, size_t n)
{
    while (n--) { char t = *a; *a++ = *b; *b++ = t; }
}

/* a heap sort: no recursion, no memory */
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    char *v = base;
    size_t start, end, root, child;
    if (n < 2 || !size) return;
    for (start = n / 2; start-- > 0;) {
        for (root = start; (child = 2 * root + 1) < n; root = child) {
            if (child + 1 < n && cmp(v + child * size, v + (child + 1) * size) < 0) child++;
            if (cmp(v + root * size, v + child * size) >= 0) break;
            swap_bytes(v + root * size, v + child * size, size);
        }
    }
    for (end = n - 1; end > 0; end--) {
        swap_bytes(v, v + end * size, size);
        for (root = 0; (child = 2 * root + 1) < end; root = child) {
            if (child + 1 < end && cmp(v + child * size, v + (child + 1) * size) < 0) child++;
            if (cmp(v + root * size, v + child * size) >= 0) break;
            swap_bytes(v + root * size, v + child * size, size);
        }
    }
}

void *bsearch(const void *key, const void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    const char *v = base;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(key, v + mid * size);
        if (!c) return (void *)(v + mid * size);
        if (c < 0) hi = mid; else lo = mid + 1;
    }
    return NULL;
}

/* A piece of code that calls fn with A4 on this copy and gives the caller
 * its A4 back: for a callback with no argument (with_arg 0) or one long. */
static const UWORD thunk0[] = {
    0x2F0C,                 /* move.l a4,-(sp) */
    0x287C, 0, 0,           /* movea.l #a4,a4 */
    0x4EB9, 0, 0,           /* jsr fn */
    0x285F,                 /* move.l (sp)+,a4 */
    0x4E75,                 /* rts */
};
static const UWORD thunk1[] = {
    0x2F0C,                 /* move.l a4,-(sp) */
    0x2F2F, 0x0008,         /* move.l 8(sp),-(sp) */
    0x287C, 0, 0,           /* movea.l #a4,a4 */
    0x4EB9, 0, 0,           /* jsr fn */
    0x588F,                 /* addq.l #4,sp */
    0x285F,                 /* move.l (sp)+,a4 */
    0x4E75,                 /* rts */
};

struct wxtk_thunk {
    struct wxtk_thunk *next;
    APTR fn;
    int with_arg;
    UWORD code[12];
};
static struct wxtk_thunk *thunks;

static APTR thunk_for(APTR fn, int with_arg)
{
    struct wxtk_thunk *t;
    const UWORD *src = with_arg ? thunk1 : thunk0;
    ULONG n = with_arg ? sizeof thunk1 : sizeof thunk0, at = with_arg ? 4 : 2;
    if (!fn) return NULL;
    for (t = thunks; t; t = t->next) if (t->fn == fn && t->with_arg == with_arg) return t->code;
    t = AllocVec(sizeof *t, MEMF_ANY);
    if (!t) return NULL;
    t->fn = fn; t->with_arg = with_arg;
    CopyMem((APTR)src, t->code, n);
    *(APTR *)&t->code[at] = my_a4();
    *(APTR *)&t->code[at + 3] = fn;
    CacheClearU();
    Forbid();
    t->next = thunks;
    thunks = t;
    Permit();
    return t->code;
}

extern int __real_pthread_once(pthread_once_t *once, void (*fn)(void));
extern int __real_pthread_key_create(pthread_key_t *key, void (*dtor)(void *));
extern int __real_pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg);

int __wrap_pthread_once(pthread_once_t *once, void (*fn)(void))
{
    void (*t)(void) = (void (*)(void))thunk_for((APTR)fn, 0);
    return __real_pthread_once(once, t ? t : fn);
}

int __wrap_pthread_key_create(pthread_key_t *key, void (*dtor)(void *))
{
    void (*t)(void *) = dtor ? (void (*)(void *))thunk_for((APTR)dtor, 1) : NULL;
    return __real_pthread_key_create(key, t ? t : dtor);
}

/* a new thread starts in wxtk_thread_start (mod_tables.s), which sets A4
 * from this and calls on */
struct wxtk_start {
    void *(*fn)(void *);
    void *arg;
    APTR a4;
};

extern void *wxtk_thread_start(void *);

/* called by wxtk_thread_start with A4 set */
void *wxtk_thread_run(struct wxtk_start *s)
{
    void *(*fn)(void *) = s->fn;
    void *arg = s->arg;
    free(s);
    return fn(arg);
}

int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg)
{
    struct wxtk_start *s = malloc(sizeof *s);
    int r;
    if (!s) return 12;      /* ENOMEM */
    s->fn = fn; s->arg = arg; s->a4 = my_a4();
    r = __real_pthread_create(t, attr, wxtk_thread_start, s);
    if (r) free(s);
    return r;
}
