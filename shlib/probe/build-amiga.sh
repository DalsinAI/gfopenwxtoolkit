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
  echo "== shapes.o"; m68k-amigaos-objdump -h "$W/t/shapes.o" | grep -v "^ *CONTENTS"; m68k-amigaos-objdump -t "$W/t/shapes.o" | grep -v " l "
  echo "== data-sections"; m68k-amigaos-g++ -noixemul -m68040 -O2 -std=gnu++17 -fbaserel32 -fdata-sections -c "$HERE/tests/library/shapes.cpp" -o "$W/t/ds.o" && m68k-amigaos-objdump -t "$W/t/ds.o" | grep -e shape_names -e made -e greeting
  m68k-amigaos-g++ -noixemul -m68040 -O2 -std=gnu++17 -fbaserel32 -S -o - "$HERE/tests/library/shapes.cpp" | grep -n -B3 -A6 "^_shape_names:"
} > "$OUTD/probe.log" 2>&1
tail -n 30 "$OUTD/build.log"
: > "$OUTD/exit-$rc"
exit 0
