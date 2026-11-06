#!/bin/sh
# Temporary: what the module imports from a program's C library (from the
# last shlib build), and which C library objects take callbacks.
# Copyright (c) 2026 AmiCradle Team
set -u
WORK="$HOME/.cache/gfopenwxtoolkit/library-build"
OUTD="${OUT:-out}"; mkdir -p "$OUTD"; LOG="$OUTD/probe.log"
PATH="$STOVE/prefix/bin:$PATH"
L="$WORK/library"
{
echo "== imports.txt"; cat "$L/imports.txt"
echo "== data sections of wx data exports (counts)"
python3 - "$L/symbols.json" <<'X'
import json,sys
d=json.load(open(sys.argv[1]))
print(len(d["exports"]), "exports", len(d["imports"]), "imports")
X
echo "== where libnix keeps errno, stdout"
grep -n "errno\|__sF\|stdout\|_impure" "$(m68k-amigaos-gcc -print-sysroot 2>/dev/null)/include/errno.h" 2>/dev/null | head
for h in errno.h stdio.h; do f=$(echo "#include <$h>" | m68k-amigaos-gcc -noixemul -E -x c - 2>/dev/null | grep -m1 "\"/.*$h\"" | sed 's/.*"\(.*\)".*/\1/'); echo "-- $f"; grep -n "errno\|stdout\|stdin\|__sF\|__stdfile" "$f" | head -12; done
} > "$LOG" 2>&1
echo done
exit 0
