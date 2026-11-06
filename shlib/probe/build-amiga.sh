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
  echo "== module code"; m68k-amigaos-objdump -d -r "$L/mod_tables.o" | sed -n '1,60p'
  m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0 --stop-address=0x80
  a=$(m68k-amigaos-nm "$L/wx.module.debug" | grep " _wxtk_module_entry$" | cut -d' ' -f1); m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0x$a --stop-address=$(printf '0x%x' $((0x$a + 0xa0)))
  a=$(m68k-amigaos-nm "$L/wx.module.debug" | grep " _getenv$" | cut -d' ' -f1); m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0x$a --stop-address=$(printf '0x%x' $((0x$a + 0x10)))
  echo "== module symbols"; m68k-amigaos-objdump -h "$L/wx.module.debug"; m68k-amigaos-objdump -t "$L/wx.module.debug" | grep -e a4_init -e data_size -e datadata -e _wxtk_module_start -e LIST__ -e EH_FRAME
  echo "== shapetest map (pulled from archives)"; grep -A1 "^Archive member" "$W/t/shapetest.map" 2>/dev/null | head -80
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
