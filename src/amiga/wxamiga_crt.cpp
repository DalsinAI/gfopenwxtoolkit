/*
 * wxamiga_crt.cpp: the C library calls wxBase needs that libnix doesn't
 * have, or has wrong: the wide-character memory and multibyte calls,
 * numbers, search, scanf and stream calls, mkstemp, and statvfs,
 * sigaction, setsid and pclose.
 *
 * Numbers: wide text is narrowed one character for one (anything past
 * 0x7f becomes 0x01, which no number contains) and given to libnix's own
 * strtoX/vsscanf, so the end pointer maps back by index.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/statvfs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dos.h>

namespace {

/* Narrows s into a buffer (on the stack when short). */
struct Narrow
{
    char small[128];
    char *p;
    size_t n;
    explicit Narrow(const wchar_t *s, size_t limit = (size_t)-1)
    {
        n = 0;
        while (s[n] && n < limit) n++;
        p = n < sizeof small ? small : (char *)malloc(n + 1);
        if (!p) { p = small; n = 0; }
        for (size_t i = 0; i < n; i++)
            p[i] = s[i] < 0x80 ? (char)s[i] : '\x01';
        p[n] = 0;
    }
    ~Narrow() { if (p != small) free(p); }
};

template <typename T, typename F>
T wide_num(const wchar_t *s, wchar_t **end, F f)
{
    Narrow nw(s);
    char *e = nw.p;
    T v = f(nw.p, &e);
    if (end) *end = (wchar_t *)(s + (e - nw.p));
    return v;
}

/* UTF-8 out, one character at a time. */
int put_utf8(wchar_t c, FILE *f)
{
    unsigned long u = (unsigned long)c;
    char b[4]; int n;
    if (u < 0x80) { b[0] = (char)u; n = 1; }
    else if (u < 0x800) { b[0] = (char)(0xc0 | (u >> 6)); b[1] = (char)(0x80 | (u & 0x3f)); n = 2; }
    else if (u < 0x10000) { b[0] = (char)(0xe0 | (u >> 12)); b[1] = (char)(0x80 | ((u >> 6) & 0x3f)); b[2] = (char)(0x80 | (u & 0x3f)); n = 3; }
    else { b[0] = (char)(0xf0 | (u >> 18)); b[1] = (char)(0x80 | ((u >> 12) & 0x3f)); b[2] = (char)(0x80 | ((u >> 6) & 0x3f)); b[3] = (char)(0x80 | (u & 0x3f)); n = 4; }
    return fwrite(b, 1, n, f) == (size_t)n ? 0 : -1;
}

} // namespace

extern "C" {

/* libnix's wmemcpy copies bytes, not wide characters (a 7-character copy
 * moved 7 bytes): wxString builds every string with it. These replace
 * libnix's whole wmem family, linked before it. */
wchar_t *wmemcpy(wchar_t *d, const wchar_t *s, size_t n)
{ return (wchar_t *)memcpy(d, s, n * sizeof(wchar_t)); }

wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n)
{ return (wchar_t *)memmove(d, s, n * sizeof(wchar_t)); }

wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n)
{
    for (size_t i = 0; i < n; i++) d[i] = c;
    return d;
}

int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (s[i] == c) return (wchar_t *)(s + i);
    return 0;
}

long wcstol(const wchar_t *s, wchar_t **end, int base)
{ return wide_num<long>(s, end, [base](const char *p, char **e) { return strtol(p, e, base); }); }

unsigned long wcstoul(const wchar_t *s, wchar_t **end, int base)
{ return wide_num<unsigned long>(s, end, [base](const char *p, char **e) { return strtoul(p, e, base); }); }

long long wcstoll(const wchar_t *s, wchar_t **end, int base)
{ return wide_num<long long>(s, end, [base](const char *p, char **e) { return strtoll(p, e, base); }); }

unsigned long long wcstoull(const wchar_t *s, wchar_t **end, int base)
{ return wide_num<unsigned long long>(s, end, [base](const char *p, char **e) { return strtoull(p, e, base); }); }

double wcstod(const wchar_t *s, wchar_t **end)
{ return wide_num<double>(s, end, [](const char *p, char **e) { return strtod(p, e); }); }

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set)
{
    for (; *s; s++)
        for (const wchar_t *k = set; *k; k++)
            if (*s == *k) return (wchar_t *)s;
    return 0;
}

size_t wcsspn(const wchar_t *s, const wchar_t *set)
{
    size_t n = 0;
    for (; s[n]; n++) {
        const wchar_t *k = set;
        while (*k && *k != s[n]) k++;
        if (!*k) break;
    }
    return n;
}

wchar_t *wcsstr(const wchar_t *h, const wchar_t *n)
{
    if (!*n) return (wchar_t *)h;
    for (; *h; h++) {
        const wchar_t *a = h, *b = n;
        while (*a && *b && *a == *b) a++, b++;
        if (!*b) return (wchar_t *)h;
    }
    return 0;
}

/* Wide scanf: numbers only. A format with a string, character or set
 * conversion (they would store wide characters) fails with EOF. */
int vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap)
{
    for (const wchar_t *f = fmt; *f; f++) {
        if (*f != L'%') continue;
        f++;
        if (*f == L'%') continue;
        while (*f && !wcschr(L"diouxXaAeEfFgGsScC[pn", *f)) f++;
        if (*f == L's' || *f == L'S' || *f == L'c' || *f == L'C' || *f == L'[') { errno = EINVAL; return EOF; }
        if (!*f) break;
    }
    Narrow ns(s), nf(fmt);
    return vsscanf(ns.p, nf.p, ap);
}

int swscanf(const wchar_t *s, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vswscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

int fputws(const wchar_t *s, FILE *f)
{
    for (; *s; s++)
        if (put_utf8(*s, f)) return -1;
    return 0;
}

int vfwprintf(FILE *f, const wchar_t *fmt, va_list ap)
{
    size_t cap = 256;
    for (;;) {
        wchar_t *b = (wchar_t *)malloc(cap * sizeof(wchar_t));
        if (!b) return -1;
        va_list cp; va_copy(cp, ap);
        int n = vswprintf(b, cap, fmt, cp);
        va_end(cp);
        if (n >= 0 && (size_t)n < cap - 1) {
            int r = fputws(b, f) ? -1 : n;
            free(b);
            return r;
        }
        free(b);
        if (cap >= 1u << 20) return -1;
        cap *= 4;
    }
}

int fwprintf(FILE *f, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int r = vfwprintf(f, fmt, ap);
    va_end(ap);
    return r;
}

/* No POSIX signals on AmigaOS: handlers are never installed. */
int sigaction(int, const struct sigaction *, struct sigaction *)
{ errno = ENOSYS; return -1; }

pid_t setsid(void) { errno = ENOSYS; return -1; }

/* libnix's popen opens a PIPE: file and has no pclose; closing the file
 * is all there is (no exit status to wait for). */
int pclose(FILE *f) { return fclose(f) ? -1 : 0; }


/* The multibyte calls, in the Amiga's own Latin-1, one byte for one
 * character. libnix's sign-extend bytes past 0x7f, stop after 65535, and
 * write through a NULL buffer when asked only for the length, which wx's
 * wxConvLibc does for every string. */
size_t mbstowcs(wchar_t *d, const char *s, size_t n)
{
    size_t i = 0;
    if (!d)
        return strlen(s);
    for (; i < n; i++) {
        d[i] = (unsigned char)s[i];
        if (!s[i])
            return i;
    }
    return i;
}

size_t wcstombs(char *d, const wchar_t *s, size_t n)
{
    size_t i = 0;
    for (;; i++) {
        wchar_t c = s[i];
        if ((unsigned long)c > 0xff) {
            errno = EILSEQ;
            return (size_t)-1;
        }
        if (!c) {
            if (d && i < n)
                d[i] = 0;
            return i;
        }
        if (d) {
            if (i >= n)
                return i;
            d[i] = (char)c;
        }
    }
}

int mbtowc(wchar_t *d, const char *s, size_t n)
{
    if (!s)
        return 0;
    if (!n)
        return -1;
    if (d)
        *d = (unsigned char)*s;
    return *s ? 1 : 0;
}

int mblen(const char *s, size_t n) { return mbtowc(NULL, s, n); }

int wctomb(char *d, wchar_t c)
{
    if (!d)
        return 0;
    if ((unsigned long)c > 0xff) {
        errno = EILSEQ;
        return -1;
    }
    *d = (char)c;
    return 1;
}

size_t mbrtowc(wchar_t *d, const char *s, size_t n, mbstate_t *)
{
    if (!s)
        return 0;
    if (!n)
        return (size_t)-2;
    if (d)
        *d = (unsigned char)*s;
    return *s ? 1 : 0;
}

size_t wcrtomb(char *d, wchar_t c, mbstate_t *)
{
    if (!d)
        return 1;
    if ((unsigned long)c > 0xff) {
        errno = EILSEQ;
        return (size_t)-1;
    }
    *d = (char)c;
    return 1;
}

wint_t btowc(int c) { return c == EOF ? WEOF : (wint_t)(unsigned char)c; }
int wctob(wint_t c) { return (unsigned long)c > 0xff ? EOF : (int)c; }

/* mkstemp: libnix's opens the file without O_RDWR, so nothing can be
 * written to it (wxTempFile, wxFileConfig). Names from the task's address
 * and a count, tried until one is free. */
int mkstemp(char *path)
{
    static unsigned long count;
    size_t len = strlen(path), x = len;
    while (x > 0 && path[x - 1] == 'X')
        x--;
    if (len - x < 6) {
        errno = EINVAL;
        return -1;
    }
    unsigned long seed = (unsigned long)FindTask(NULL) ^ ((unsigned long)time(NULL) << 8);
    for (int tries = 0; tries < 1000; tries++) {
        unsigned long v = seed + ++count * 7919;
        for (size_t i = x; i < len; i++) {
            path[i] = "0123456789abcdefghijklmnopqrstuvwxyz"[v % 36];
            v /= 36;
        }
        /* libnix's open with O_EXCL fails with ENOENT for a new file:
         * the name is checked with a lock, then made */
        BPTR lock = Lock((STRPTR)path, ACCESS_READ);
        if (lock) {
            UnLock(lock);
            continue;
        }
        return open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    }
    return -1;
}

/* mktime: libnix's takes the fields as UTC, while its localtime takes the
 * locale's zone off (_timezone, seconds west of GMT), so a local time made
 * and read back came out a zone away (wxDateTime from day and hour). The
 * fields are normalised, made seconds since 1970 as local time, and given
 * back filled as localtime would (no summer time: AmigaOS has none). */
time_t mktime(struct tm *tm)
{
    long long y = tm->tm_year + 1900LL, mon = tm->tm_mon;
    y += mon / 12;
    mon %= 12;
    if (mon < 0) { mon += 12; y--; }
    /* days from 1970-01-01 to the first of the month (civil calendar) */
    long long yy = y - (mon < 2);
    long long era = (yy >= 0 ? yy : yy - 399) / 400;
    long long yoe = yy - era * 400;
    long long mp = (mon + 10) % 12;     /* months from March (mon 0 is January) */
    long long doy = (153 * mp + 2) / 5;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468 + (tm->tm_mday - 1);
    long long secs = days * 86400LL + tm->tm_hour * 3600LL + tm->tm_min * 60LL + tm->tm_sec;
    time_t t = (time_t)(secs + _timezone);
    struct tm *lt = localtime(&t);
    if (!lt) return (time_t)-1;
    *tm = *lt;
    tm->tm_isdst = 0;
    return t;
}

} // extern "C"

/* sys/statvfs.h declares it without C linkage. Filled from Info() on a
 * lock of the path. */
__stdargs int statvfs(const char *path, struct statvfs *st)
{
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock) { errno = ENOENT; return -1; }
    struct InfoData *id = (struct InfoData *)AllocVec(sizeof(struct InfoData), MEMF_PUBLIC | MEMF_CLEAR);
    int ok = id && Info(lock, id);
    UnLock(lock);
    if (!ok) { FreeVec(id); errno = EIO; return -1; }
    memset(st, 0, sizeof *st);
    st->f_bsize = st->f_frsize = id->id_BytesPerBlock;
    st->f_blocks = id->id_NumBlocks;
    st->f_bfree = st->f_bavail = id->id_NumBlocks - id->id_NumBlocksUsed;
    st->f_namemax = 107;
    st->f_flag = id->id_DiskState == ID_WRITE_PROTECTED ? 1 : 0;
    FreeVec(id);
    return 0;
}

/* wxExecute's AmigaOS half (src/unix/utilsunx.cpp, patch 0005): runs one
 * command line through dos.library. sync: waits and returns its return code
 * (-1 when it could not start). async: starts it in a CLI of its own with
 * NIL: in and out, and returns a number that is never 0 (AmigaOS has no
 * process ids), or 0 when it could not start. The child gets the caller's
 * stack size, at least 64 KB. */
#include <dos/dostags.h>
extern "C" long wxAmigaExecuteLine(const char *line, int sync)
{
    struct Task *me = FindTask(NULL);
    ULONG stack = (ULONG)((char *)me->tc_SPUpper - (char *)me->tc_SPLower);
    if (stack < 65536)
        stack = 65536;

    if (sync)
    {
        LONG rc = SystemTags((CONST_STRPTR)line, NP_StackSize, stack, TAG_DONE);
        return rc;
    }

    BPTR in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    if (!in || !out)
    {
        if (in) Close(in);
        if (out) Close(out);
        return 0;
    }
    LONG rc = SystemTags((CONST_STRPTR)line,
                         SYS_Input, in,
                         SYS_Output, out,
                         SYS_Asynch, TRUE,
                         NP_StackSize, stack,
                         TAG_DONE);
    if (rc == -1)
    {
        /* not started: the handles are still ours */
        Close(in);
        Close(out);
        return 0;
    }
    static long started = 0;
    return ++started;
}
