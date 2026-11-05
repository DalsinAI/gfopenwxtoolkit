#!/bin/sh
# Probe: what the stove offers for sockets (temporary, not for merge)
P="$STOVE/prefix"; T=$P/m68k-amigaos; I=$T/libnix/include
L=$T/libnix/lib/libm020/libsocket.a
echo "== nm rest"; "$P/bin/m68k-amigaos-nm" "$L" | grep -E ' [TDBC] ' | sed -n '80,200p' | tr '\n' ' '; echo
echo "== socket.h"; grep -v '^\s*$' "$I/socket.h"
echo "== stdfiledes"; f=$(find "$T" -name 'stdfiledes.h' | head -1); echo $f; grep -v '^\s*$' "$f" | head -80
echo "== proto/bsdsocket"; find "$T" -path '*proto/bsdsocket.h' -o -path '*inline/bsdsocket.h' -o -path '*proto/socket.h' | head
echo "== sys/filio"; find "$T" -name 'filio.h' -o -name 'ioctl.h' | head
echo "== sys/socket protos"; grep -n '__stdargs\|^extern\|int socket\|connect' "$I/sys/socket.h" | head -30
mkdir -p "$OUT"; echo probe > "$OUT/probe.txt"
