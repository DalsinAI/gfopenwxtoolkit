/*
 * wxamiga_stubs.c: Unix calls wx's Unix code links to that an Amiga
 * program without a TCP/IP stack or users has no use for. Each says it
 * isn't there (NULL, or -1 with ENOSYS), so wx takes its other way: the
 * user's name from USER, the host's from HOSTNAME.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#ifndef __stdargs
#define __stdargs
#endif

void *getpwnam(const char *name) { (void)name; return NULL; }
void *getpwuid(int uid) { (void)uid; return NULL; }
void *gethostbyname(const char *name) { (void)name; return NULL; }

__stdargs int select(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
    (void)n; (void)r; (void)w; (void)e; (void)t;
    errno = ENOSYS;
    return -1;
}
