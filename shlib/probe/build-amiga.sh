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
{ echo "== exit $rc"; tail -n 80 "$OUTD/build.log"
  for f in imports.txt exports.txt; do echo "== $f"; head -c 20000 "$L/$f" 2>/dev/null; done
  echo "== mod1.log"; grep -v 'duplicate section' "$L/mod1.log" 2>/dev/null | head -60
  echo "== mod_tables.s head"; head -40 "$L/mod_tables.s" 2>/dev/null
  echo "== libnix resident start"; for f in nrcrt0.o; do p=$(m68k-amigaos-gcc -noixemul -fbaserel32 -resident32 -print-file-name=$f); echo "-- $p"; m68k-amigaos-objdump -d -r "$p" | head -150; done
  echo "== __init_eh"; for a in $(m68k-amigaos-gcc -noixemul -fbaserel32 -print-file-name=libnix.a) $(m68k-amigaos-gcc -noixemul -fbaserel32 -print-file-name=libstubs.a) $(m68k-amigaos-gcc -noixemul -fbaserel32 -print-libgcc-file-name); do m68k-amigaos-nm -A "$a" 2>/dev/null | grep -e "T ___init_eh" -e "T ___exit_eh" -e "D ___EH_FRAME" -e "T ___register_frame_info$"; done
  d=$(mktemp -d); cd "$d"; for a in $(m68k-amigaos-gcc -noixemul -fbaserel32 -print-file-name=libnix.a) $(m68k-amigaos-gcc -noixemul -fbaserel32 -print-file-name=libstubs.a); do m=$(m68k-amigaos-nm -A "$a" 2>/dev/null | grep "T ___init_eh" | head -1 | sed 's/^[^:]*:\([^:]*\):.*/\1/'); [ -n "$m" ] && m68k-amigaos-ar x "$a" "$m" && m68k-amigaos-objdump -d -r -h "$m" | head -120; done; cd - >/dev/null
  echo "== shapes.o lists"; m68k-amigaos-objdump -h -r "$W/t/shapes.o" | grep -i -A3 "list"
  echo "== module symbols"; m68k-amigaos-objdump -h "$L/wx.module.debug"; m68k-amigaos-objdump -t "$L/wx.module.debug" | grep -e a4_init -e data_size -e datadata -e _wxtk_module_start -e LIST__ -e EH_FRAME
  echo "== shapetest map (pulled from archives)"; grep -A1 "^Archive member" "$W/t/shapetest.map" 2>/dev/null | head -80
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
