#!/bin/sh
# Temporary: scripts/test-library.sh on the stove, its log into OUT.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUTD="${OUT:-out}"; mkdir -p "$OUTD"
PATH="$STOVE/prefix/bin:$PATH"
W="$HOME/.cache/gfopenwxtoolkit/libtest"
rm -rf "$W"
WORK="$W" OUT="$OUTD" timeout 800 sh "$HERE/scripts/test-library.sh" > "$OUTD/build.log" 2>&1
rc=$?
L="$W/library"
{ echo "== exit $rc"; grep -v -e warning -e "^ *|" -e "^ *[0-9]* |" "$OUTD/build.log" | tail -n 20
  echo "== constructors"; m68k-amigaos-nm -n "$L/wx.module.debug" > "$W/nm.txt"
  c=$(grep " ___CTOR_LIST__$" "$W/nm.txt" | cut -d' ' -f1); d=$(grep " ___DTOR_LIST__$" "$W/nm.txt" | cut -d' ' -f1)
  m68k-amigaos-objdump -s -j .text --start-address=0x$c --stop-address=0x$d "$L/wx.module.debug" | grep '^ ' > "$W/ctor.txt"
  python3 - "$W/nm.txt" "$W/ctor.txt" <<'P'
import sys
syms=[]
for l in open(sys.argv[1]):
    t=l.split()
    if len(t)==3 and t[1] in "tTdDbB": syms.append((int(t[0],16),t[2]))
words=[]
for l in open(sys.argv[2]):
    t=l.split()
    words+= [int(w,16) for w in t[1:5] if len(w)==8]
for w in words:
    best=max((s for s in syms if s[0]<=w), default=(0,"?"))
    print("%08x %s+%x" % (w,best[1],w-best[0]))
P
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
