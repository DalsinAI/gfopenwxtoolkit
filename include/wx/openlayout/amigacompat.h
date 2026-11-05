/*
 * amigacompat.h: included first in every wxWidgets file built for
 * m68k-amigaos (-include). It brings in the libnix/pthreads headers once
 * and takes away pthread.h's cancellation-point wrappers, which are
 * function-like macros (read(...) and the like) and break wx's
 * "::read" and its wxFile::read members. Threads in this port are never
 * cancelled (wxThread::Delete asks; it doesn't cancel), so nothing is lost.
 *
 * Copyright (c) 2026 AmiCradle Team
 */
#ifndef GFOPENWX_AMIGACOMPAT_H
#define GFOPENWX_AMIGACOMPAT_H

#ifdef __cplusplus
#include <cstddef>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

#undef close
#undef fsync
#undef read
#undef sleep
#undef usleep
#undef creat
#undef fcntl
#undef open
#undef nanosleep
#undef readv

#ifdef __cplusplus
extern "C" {
#endif
/* libnix has it (libpthread's time support) but time.h only declares it
 * under the POSIX timers switch. */
__stdargs int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
#ifdef __cplusplus
}
#endif

/* libnix's dlfcn.h (dlopen in libnix.a) has no extern "C" of its own:
 * include it here once, with C linkage, so wx's dlunix.cpp links. */
#ifdef __cplusplus
extern "C" {
#endif
#include <dlfcn.h>
#ifdef __cplusplus
}
#endif

#endif
