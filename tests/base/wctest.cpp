#include "wx/filename.h"
#include "wx/datetime.h"
#include <unistd.h>
#include <errno.h>
#include <time.h>
// wctest: the C library's wide-character calls and wx's conversions, one
// line each, to find where wide text goes wrong on AmigaOS.
// Copyright (c) 2026 AmiCradle Team
#include <wx/init.h>
#include <wx/string.h>
#include <wx/strconv.h>
#include <wx/tokenzr.h>
#include <wx/filename.h>
#include <wchar.h>
#include <cstdio>
#include <pthread.h>
#include <sys/time.h>
#include <wx/time.h>
#include <cstring>

int main(int argc, char **argv)
{
    // each line as it is made, so a hang shows where it stopped
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("     starting wx\n");
    wxInitializer init(argc, argv);
    const wchar_t *w = L"abc,def";
    printf("sizeof(wchar_t)=%d L'a'=%ld\n", (int)sizeof(wchar_t), (long)w[0]);
    printf("wcslen=%d (7)\n", (int)wcslen(w));
    printf("wcschr ,=%d (3)\n", wcschr(w, L',') ? (int)(wcschr(w, L',') - w) : -1);
    printf("wmemchr d=%d (4)\n", wmemchr(w, L'd', 7) ? (int)((const wchar_t *)wmemchr(w, L'd', 7) - w) : -1);
    printf("wcspbrk=%d (3)\n", wcspbrk(w, L",;") ? (int)(wcspbrk(w, L",;") - w) : -1);
    printf("wcsspn=%d (3)\n", (int)wcsspn(w, L"abc"));
    printf("wcscmp=%d (0)\n", wcscmp(w, L"abc,def"));
    printf("wmemcmp=%d (0)\n", wmemcmp(w, L"abc,def", 7));
    wchar_t buf[16]; wmemcpy(buf, w, 8);
    printf("wmemcpy ok=%d\n", wcscmp(buf, w) == 0);
    char nb[32];
    size_t n = wcstombs(nb, w, sizeof nb);
    printf("wcstombs=%d '%s'\n", (int)n, n < 32 ? nb : "");
    wchar_t wb[32];
    n = mbstowcs(wb, "xyz", 32);
    printf("mbstowcs=%d first=%ld\n", (int)n, (long)wb[0]);

    wxString s(w);
    printf("wxString len=%d (7)\n", (int)s.length());
    printf("find ,=%d (3)\n", (int)s.find(','));
    printf("find_first_of ,;=%d (3)\n", (int)s.find_first_of(",;"));
    printf("Find(def)=%d (4)\n", s.Find("def"));
    wxStringTokenizer tk(s, ",");
    printf("tokens=%d (2)\n", (int)tk.CountTokens());
    wxCharBuffer u = wxConvUTF8.cWC2MB(w);
    printf("UTF8 cWC2MB len=%d '%s'\n", (int)u.length(), u.data() ? u.data() : "(null)");
    printf("utf8_str len=%d\n", (int)s.utf8_str().length());
    printf("mb_str len=%d\n", (int)s.mb_str().length());
    wxString e(L"été");
    printf("e len=%d utf8=%d\n", (int)e.length(), (int)e.utf8_str().length());
    printf("ToStdString='%s'\n", s.ToStdString().c_str());
    wxFileName fn("RAM:T/x/y.txt");
    printf("fn path='%s' full='%s' dirs=%d\n", (const char *)fn.GetPath().mb_str(), (const char *)fn.GetFullPath().mb_str(), (int)fn.GetDirCount());
    printf("fn.fn_str='%s'\n", (const char *)fn.GetFullPath().fn_str());
    // strftime and mkstemp, as wxDateTime::Format and wxTempFile call them
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        const char *fmts[] = { "%Y-%m-%d", "%H:%M:%S", "%a %b", "%c", "%x", "%X", "%p", "%z", "%Z", "%%" };
        for (size_t i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
            char b[256];
            size_t n = strftime(b, sizeof b, fmts[i], t);
            printf("strftime(%s)=%u '%s'\n", fmts[i], (unsigned)n, n ? b : "");
        }
        char big[4096];
        printf("strftime 4096=%u\n", (unsigned)strftime(big, sizeof big, "%Y", t));
        {
            struct timeval tv; gettimeofday(&tv, NULL);
            time_t now = time(NULL);
            printf("time=%ld gettimeofday=%ld wxUTCms/1000=%ld _timezone=%ld wxGetTimeZone=%d\n",
                   (long)now, (long)tv.tv_sec, (long)(wxGetUTCTimeMillis() / 1000).ToLong(),
                   (long)_timezone, wxGetTimeZone());
            struct tm lt = *localtime(&now);
            printf("localtime isdst=%d %02d:%02d\n", lt.tm_isdst, lt.tm_hour, lt.tm_min);
            struct tm m = {}; m.tm_year = 126; m.tm_mon = 10; m.tm_mday = 4; m.tm_hour = 18; m.tm_min = 9; m.tm_sec = 52; m.tm_isdst = -1;
            time_t mk = mktime(&m);
            struct tm back = *localtime(&mk);
            printf("mktime(18:09:52)=%ld isdst=%d back=%02d:%02d:%02d\n", (long)mk, m.tm_isdst, back.tm_hour, back.tm_min, back.tm_sec);
            wxDateTime d(4, wxDateTime::Nov, 2026, 18, 9, 52);
            printf("wxDateTime(18:09:52) ticks=%ld IsDST=%d Format=%s\n", (long)d.GetTicks(), d.IsDST(),
                   (const char *)d.Format("%H:%M:%S").utf8_str());
        }
        {
            long v = 0;
            bool ok = wxString("44100").ToLong(&v);
            const wchar_t num[] = L"-123x";
            wchar_t *e = NULL;
            long w = wcstol(num, &e, 10);
            printf("ToLong=%d %ld wcstol=%ld end=%d\n", (int)ok, v, w, e ? (int)(e - num) : -1);
        }
        {
            pthread_mutexattr_t at;
            pthread_mutexattr_init(&at);
            pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
            pthread_mutex_t m1, m2, m3;
            pthread_mutex_init(&m1, &at);
            int d1 = pthread_mutex_destroy(&m1);
            pthread_mutex_init(&m2, &at);
            pthread_mutex_lock(&m2); pthread_mutex_unlock(&m2);
            int d2 = pthread_mutex_destroy(&m2);
            pthread_mutex_init(&m3, NULL);
            pthread_mutex_lock(&m3); pthread_mutex_unlock(&m3);
            int d3 = pthread_mutex_destroy(&m3);
            printf("mutex destroy: recursive unused=%d recursive used=%d plain used=%d\n", d1, d2, d3);
        }
        printf("Format=%s\n", (const char *)wxDateTime::Now().Format("%Y-%m-%d %H:%M").utf8_str());
        char tmpl[] = "RAM:T/wxtmpXXXXXX";
        int fd = mkstemp(tmpl);
        printf("mkstemp=%d errno=%d '%s'\n", fd, fd < 0 ? errno : 0, tmpl);
        if (fd >= 0) { printf("write=%d\n", (int)write(fd, "hi", 2)); close(fd); remove(tmpl); }
        wxString tn = wxFileName::CreateTempFileName("RAM:T/wxt");
        printf("CreateTempFileName='%s'\n", (const char *)tn.utf8_str());
        if (!tn.empty()) wxRemoveFile(tn);
    }
    printf("wctest: done\n");
    return 0;
}
