/* temporary: tells how far a program's start gets (libnix's init list,
 * the C++ constructors), each line written straight to Output() */
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stabs.h>
void tr_say(const char *s) { BPTR o = Output(); if (o) Write(o, (APTR)s, strlen(s)); }
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
