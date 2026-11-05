#!/bin/sh
# Probe: what the stove offers for sockets (temporary, not for merge)
P="$STOVE/prefix"; T=$P/m68k-amigaos; I=$T/libnix/include
echo "== StdFileDes"; grep -n -B3 -A25 'StdFileDes' "$I/stdio.h" | head -80
echo "== fd syms"; "$P/bin/m68k-amigaos-nm" "$T/libnix/lib/libm020/libnix.a" 2>/dev/null | grep -iE ' [TDBC] .*(fhfromfd|stdfiledes|__stdfiledes|_lx_|ObtainSocket|fd2)' | sort -u | head -20
echo "== select.o"; cd /tmp && rm -rf selp && mkdir selp && cd selp && "$P/bin/m68k-amigaos-ar" x "$T/libnix/lib/libm020/libsocket.a" select.o 2>&1; ls; "$P/bin/m68k-amigaos-nm" select.o 2>&1; "$P/bin/m68k-amigaos-objdump" -d select.o 2>&1 | head -150
echo "== inline WaitSelect"; grep -n -A6 'define WaitSelect' "$T/include/inline/bsdsocket.h"; grep -n 'SocketBase' "$T/include/proto/bsdsocket.h"
mkdir -p "$OUT"; echo probe > "$OUT/probe.txt"
