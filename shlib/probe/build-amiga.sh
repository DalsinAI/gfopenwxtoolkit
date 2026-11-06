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
  echo "== module code"; m68k-amigaos-objdump -d -r "$L/mod_tables.o" | sed -n '1,60p'
  m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0 --stop-address=0x80
  a=$(m68k-amigaos-nm "$L/wx.module.debug" | grep " _wxtk_module_entry$" | cut -d' ' -f1); m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0x$a --stop-address=$(printf '0x%x' $((0x$a + 0xa0)))
  a=$(m68k-amigaos-nm "$L/wx.module.debug" | grep " _getenv$" | cut -d' ' -f1); m68k-amigaos-objdump -d "$L/wx.module.debug" --start-address=0x$a --stop-address=$(printf '0x%x' $((0x$a + 0x10)))
  m68k-amigaos-nm "$L/wx.module.debug" | grep -e " __wxtk_s0$" -e "___a4_init" -e "Lslots"
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
