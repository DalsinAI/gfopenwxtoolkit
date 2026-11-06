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
  echo "== init list entries in the program's libraries"
  d=$(mktemp -d); cd "$d"
  for l in libpthread.a libnix.a libstubs.a libnix20.a libamiga.a libsocket.a libatomic.a; do
    a=$(m68k-amigaos-gcc -noixemul -m68040 -print-file-name=$l); [ -f "$a" ] || continue
    rm -f ./*; m68k-amigaos-ar x "$a" 2>/dev/null
    for o in *.o; do
      m68k-amigaos-objdump -h "$o" 2>/dev/null | grep -q "list___INIT_LIST__" || continue
      r=$(m68k-amigaos-objdump -r -j .list___INIT_LIST__ "$o" | awk '/RELOC32/{print $3}' | tr '\n' ' ')
      v=$(m68k-amigaos-objdump -s -j .list___INIT_LIST__ "$o" | grep '^ 0000' | awk '{print $3}')
      echo "$l $o: $r pri $v"
    done
  done; cd - >/dev/null
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
