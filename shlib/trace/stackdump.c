/* (temporary) StackDump NAME: for each process whose CLI command name holds
 * NAME, its stack words that point into its own code or wx.module's, as
 * hunk number + offset, so a stuck program shows where it waits.
 * Copyright (c) 2026 AmiCradle Team */
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>

struct WXTKBase { struct Library lib; BPTR seglist; APTR sys; APTR dos; struct SignalSemaphore lock; BPTR module; ULONG users; };

#define MAXH 64
struct seg { ULONG lo, hi; int n; char who; };
static struct seg segs[2 * MAXH]; static int nseg;

static void addsegs(BPTR sl, char who)
{
    int n = 0;
    while (sl && nseg < 2 * MAXH) {
        ULONG *p = (ULONG *)BADDR(sl);
        segs[nseg].lo = (ULONG)(p + 1); segs[nseg].hi = (ULONG)p - 4 + p[-1];
        segs[nseg].n = n++; segs[nseg].who = who;
        printf("%c hunk %d at %08lx..%08lx\n", who, segs[nseg].n, segs[nseg].lo, segs[nseg].hi);
        nseg++; sl = *p;
    }
}

static ULONG words[4096];

int main(int argc, char **argv)
{
    struct ExecBase *eb = *(struct ExecBase **)4;
    struct WXTKBase *wb;
    struct Node *n;
    struct List *lists[2];
    int k;
    if (argc < 2) { printf("StackDump NAME\n"); return 10; }
    Forbid();
    wb = (struct WXTKBase *)FindName(&eb->LibList, (STRPTR)"openwxtoolkit.library");
    Permit();
    if (wb && wb->module) addsegs(wb->module, 'M'); else printf("no wx.module loaded\n");
    lists[0] = &eb->TaskWait; lists[1] = &eb->TaskReady;
    for (k = 0; k < 2; k++) {
        Forbid();
        for (n = lists[k]->lh_Head; n->ln_Succ; n = n->ln_Succ) {
            struct Task *t = (struct Task *)n;
            struct Process *pr = (struct Process *)t;
            struct CommandLineInterface *cli;
            char name[64]; ULONG sp, up, cnt, i; int base = nseg;
            UBYTE *b;
            if (t->tc_Node.ln_Type != NT_PROCESS || !pr->pr_CLI) continue;
            cli = BADDR(pr->pr_CLI);
            if (!cli->cli_Module) continue;
            b = BADDR(cli->cli_CommandName);
            if (!b) continue;
            memcpy(name, b + 1, b[0] < 63 ? b[0] : 63); name[b[0] < 63 ? b[0] : 63] = 0;
            if (!strstr(name, argv[1])) continue;
            sp = (ULONG)t->tc_SPReg; up = (ULONG)t->tc_SPUpper;
            cnt = (up - sp) / 4; if (cnt > 4096) cnt = 4096;
            memcpy(words, (APTR)sp, cnt * 4);
            Permit();
            printf("process %08lx '%s' state %ld sigwait %08lx sp %08lx upper %08lx\n", (ULONG)t, name, (LONG)t->tc_State, t->tc_SigWait, sp, up);
            addsegs(cli->cli_Module, 'P');
            for (i = 0; i < cnt; i++) {
                ULONG v = words[i]; int s;
                if (v & 1) continue;
                for (s = 0; s < nseg; s++)
                    if (v >= segs[s].lo && v < segs[s].hi) {
                        printf("sp+%04lx %08lx %c%d+%lx\n", i * 4, v, segs[s].who, segs[s].n, v - segs[s].lo);
                        break;
                    }
            }
            nseg = base;
            Forbid();
        }
        Permit();
    }
    return 0;
}
