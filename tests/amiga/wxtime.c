/* wxtime: runs a command N times and prints each run's time in ms (E-clock),
 * for the toolkit's start-time figures. wxtime N command [args]
 * Copyright (c) 2026 AmiCradle Team */
#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Device *TimerBase;

int main(int argc, char **argv)
{
    struct timerequest tr;
    struct EClockVal a, b;
    char cmd[512] = "";
    int n, i;
    unsigned long total = 0;
    if (argc < 3) { printf("wxtime N command [args]\n"); return 10; }
    n = atoi(argv[1]);
    for (i = 2; i < argc; i++) {
        if (strlen(cmd) + strlen(argv[i]) + 2 >= sizeof cmd) break;
        strcat(cmd, argv[i]); strcat(cmd, " ");
    }
    memset(&tr, 0, sizeof tr);
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0)) return 20;
    TimerBase = tr.tr_node.io_Device;
    for (i = 0; i < n; i++) {
        ULONG f = ReadEClock(&a);
        LONG rc = SystemTags((CONST_STRPTR)cmd, SYS_Output, Open((CONST_STRPTR)"NIL:", MODE_NEWFILE), TAG_DONE);
        unsigned long long t0, t1, ms;
        ReadEClock(&b);
        t0 = ((unsigned long long)a.ev_hi << 32) | a.ev_lo;
        t1 = ((unsigned long long)b.ev_hi << 32) | b.ev_lo;
        ms = (t1 - t0) * 1000ULL / f;
        total += (unsigned long)ms;
        printf("wxtime: run %d %lu ms rc %ld\n", i + 1, (unsigned long)ms, (long)rc);
    }
    printf("wxtime: %s average %lu ms\n", cmd, n ? total / n : 0);
    CloseDevice((struct IORequest *)&tr);
    return 0;
}
