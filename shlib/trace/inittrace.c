/* temporary: tells how far a program's start gets (libnix's init list,
 * the C++ constructors), each line written straight to Output() */
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stabs.h>
extern void *__wxtk_a4;
void tr_say(const char *s)
{
    static const char hex[] = "0123456789abcdef";
    char b[16]; unsigned long v = (unsigned long)__wxtk_a4; int i;
    BPTR o = Output();
    if (!o) return;
    Write(o, (APTR)s, strlen(s) - 1);
    b[0] = ' '; b[1] = 'A'; b[2] = '4'; b[3] = ' ';
    for (i = 0; i < 8; i++) b[4 + i] = hex[(v >> (28 - 4 * i)) & 15];
    b[12] = '\n';
    Write(o, b, 13);
}
#define AT(n) void tr_##n(void) { tr_say("init: past " #n "\n"); }
AT(m7) AT(m4) AT(m3) AT(m1) AT(z0) AT(p50)
ADD2INIT(tr_m7, -7);
ADD2INIT(tr_m4, -4);
ADD2INIT(tr_m3, -3);
ADD2INIT(tr_m1, -1);
ADD2INIT(tr_z0, 0);
ADD2INIT(tr_p50, 50);
__attribute__((constructor(101))) static void tr_ctor_first(void) { tr_say("init: first C++ constructor\n"); }
__attribute__((constructor)) static void tr_ctor_last(void) { tr_say("init: a late C++ constructor\n"); }
