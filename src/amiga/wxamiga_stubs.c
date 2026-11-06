/*
 * wxamiga_stubs.c: Unix calls wx's Unix code links to that AmigaOS lacks
 * or that libnix gets wrong.
 *
 * - getpwnam, getpwuid: an Amiga has no users. NULL, so wx takes the
 *   user's name from USER.
 * - select: libnix's (-lsocket) never returns when it waits on a socket
 *   with a time limit (a connecting socket in the lab, 5 Nov 2026). This
 *   one waits with bsdsocket.library's WaitSelect on the stack's own
 *   descriptors. Descriptors that are not sockets (files, the console)
 *   count as ready, as a file is to select() on Unix.
 * - wxAmigaExePath: the running program's full path (its drawer from
 *   GetProgramDir, its name from GetProgramName), for wxStandardPaths'
 *   GetExecutablePath, where Linux reads /proc/self/exe.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>          /* libnix's StdFileDes */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <exec/types.h>
#include <proto/bsdsocket.h>
#include <proto/dos.h>

#ifndef __stdargs
#define __stdargs
#endif

void *getpwnam(const char *name) { (void)name; return NULL; }
void *getpwuid(int uid) { (void)uid; return NULL; }

int wxAmigaExePath(char *buf, long len)
{
    char name[256];
    BPTR dir = GetProgramDir();
    if (!dir || !GetProgramName((STRPTR)name, sizeof name) || !NameFromLock(dir, (STRPTR)buf, len))
        return 0;
    return AddPart((STRPTR)buf, FilePart((STRPTR)name), len) ? 1 : 0;
}

/* libnix's socket library opens it for the main task */
extern struct Library *SocketBase;

static int stack_socket(int fd)
{
    StdFileDes *sfd = _lx_fhfromfd(fd);
    if (!sfd || !(sfd->lx_flags & LX_SOCKET))
        return -1;
    return sfd->lx_sock;
}

__stdargs int select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    fd_set sr, sw, se;
    int fd, s, top = 0, plain = 0, got;
    struct timeval zero = { 0, 0 };

    if (n < 0 || n > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
    FD_ZERO(&sr);
    FD_ZERO(&sw);
    FD_ZERO(&se);
    /* the stack's descriptor for each one asked about */
    for (fd = 0; fd < n; fd++) {
        const int inR = r && FD_ISSET(fd, r);
        const int inW = w && FD_ISSET(fd, w);
        const int inE = e && FD_ISSET(fd, e);
        if (!inR && !inW && !inE)
            continue;
        s = stack_socket(fd);
        if (s < 0 || s >= FD_SETSIZE) {
            if (inR || inW)
                plain++;
            continue;
        }
        if (inR) FD_SET(s, &sr);
        if (inW) FD_SET(s, &sw);
        if (inE) FD_SET(s, &se);
        if (s + 1 > top)
            top = s + 1;
    }

    got = 0;
    if (top > 0) {
        if (!SocketBase) {
            errno = ENOSYS;
            return -1;
        }
        /* a plain descriptor is ready already: only look at the sockets */
        got = WaitSelect(top, &sr, &sw, &se, (void *)(plain ? &zero : t), NULL);
        if (got < 0)
            return -1;
    } else if (!plain && t && (t->tv_sec || t->tv_usec)) {
        /* nothing to wait on but the time */
        struct timeval left = *t;
        if (SocketBase) {
            WaitSelect(0, NULL, NULL, NULL, (void *)&left, NULL);
        }
    }

    /* the answers, by the program's own descriptors */
    got = 0;
    for (fd = 0; fd < n; fd++) {
        const int inR = r && FD_ISSET(fd, r);
        const int inW = w && FD_ISSET(fd, w);
        const int inE = e && FD_ISSET(fd, e);
        if (!inR && !inW && !inE)
            continue;
        s = stack_socket(fd);
        if (s < 0 || s >= FD_SETSIZE) {
            if (inE) FD_CLR(fd, e);
            if (inR || inW)
                got++;
            continue;
        }
        if (inR && !FD_ISSET(s, &sr)) FD_CLR(fd, r);
        if (inW && !FD_ISSET(s, &sw)) FD_CLR(fd, w);
        if (inE && !FD_ISSET(s, &se)) FD_CLR(fd, e);
        if ((inR && FD_ISSET(fd, r)) || (inW && FD_ISSET(fd, w)) || (inE && FD_ISSET(fd, e)))
            got++;
    }
    return got;
}
