#!/bin/sh
# Probe: what the stove offers for sockets (temporary, not for merge)
P="$STOVE/prefix"; T=$P/m68k-amigaos
echo "== libs"; find "$P" -name 'libsocket*' -o -name 'libnet*' -o -name 'libamiga*' | head
echo "== headers"; for h in sys/socket.h proto/bsdsocket.h inline/bsdsocket.h sys/filio.h netdb.h sys/select.h netinet/in.h arpa/inet.h; do f=$(find "$T" -path "*/$h" | head -3); echo "$h: $f"; done
for l in $(find "$P" -name 'libsocket.a'); do echo "== nm $l"; "$P/bin/m68k-amigaos-nm" "$l" | grep -E ' [TDBC] ' | head -80; done
echo "== sys/socket.h"; f=$(find "$T" -path '*/libnix/include/sys/socket.h' | head -1); [ -n "$f" ] || f=$(find "$T" -path '*/sys/socket.h' | head -1); sed -n 1,200p "$f" | grep -vE '^\s*\*|^\s*$' | head -120
echo "== sys/select.h"; f=$(find "$T" -path '*/sys/select.h' | head -1); grep -n 'select\|include' "$f" | head
echo "== unistd close"; grep -rn 'socket' $(find "$T" -path '*/libnix/include/*' -name '*.h') 2>/dev/null | grep -v 'sys/socket.h' | head -20
mkdir -p "$OUT"; echo probe > "$OUT/probe.txt"
