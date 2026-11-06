#!/bin/sh
# Development builds for openwxtoolkit.library (temporary), each in a tree of
# its own so the wx ports' build tree isn't touched:
#   o2       -O2, regex.library and OXP (Dale's pick 2 alone)
#   os       -Os, -O2 hot files, wxDEBUG_LEVEL=0 (picks 1 and 2)
#   baserel  as os, -fbaserel32, and the library, its module and programs on it
# The figures before the picks are the ports' tree's (read only). Stops before
# Cradle's 900 s limit; run it again to go on: a finished variant is skipped.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
C="$HOME/.cache/gfopenwxtoolkit"; F="$C/figures"; mkdir -p "$F"
OUTD="${OUT:-out}"; mkdir -p "$OUTD"
SHA=$(git -C "$HERE" rev-parse HEAD 2>/dev/null || echo none)
START=$(date +%s)
PATH="${STOVE:?}/prefix/bin:$PATH"

# before: the ports' -O2 build (PCRE, Expat), kept once
if [ ! -f "$F/before/sizes.txt" ] && [ -d "$C/build/out" ]; then
    mkdir -p "$F/before"
    for f in minimal basetest wctest; do [ -f "$C/build/out/$f" ] && cp "$C/build/out/$f" "$F/before/"; done
    { ls -l "$C/build/out"; ls -l "$C/build/base/lib" "$C/build/gui"/*.a; } > "$F/before/sizes.txt" 2>&1
fi

figures() {   # variant work
    d="$F/$1"; rm -rf "$d"; mkdir -p "$d"
    for f in minimal basetest wctest; do [ -f "$2/out/$f" ] && cp "$2/out/$f" "$d/"; done
    [ -d "$2/out/shared" ] && cp -a "$2/out/shared" "$d/"
    [ -d "$2/out/Libs" ] && cp -a "$2/out/Libs" "$d/"
    { ls -l "$2/out" "$2/out/shared" "$2/out/Libs" "$2/out/Libs/openwxtoolkit" 2>/dev/null
      ls -l "$2/base/lib" "$2/gui"/*.a
      m68k-amigaos-size -t "$2/gui/libwx_openlayoutu_core.a" | tail -1
      for l in "$2"/base/lib/*.a; do echo "$(basename "$l") $(m68k-amigaos-size -t "$l" | tail -1)"; done
    } > "$d/sizes.txt" 2>&1
}

for v in o2 os baserel; do
    [ "$(cat "$F/$v/.done" 2>/dev/null)" = "$SHA" ] && { echo "$v: done already"; continue; }
    left=$((830 - ($(date +%s) - START)))
    [ "$left" -gt 60 ] || { echo "out of time before $v"; break; }
    case $v in
        o2) env="WXTK_OPT=O2"; W="$C/library-build-o2" ;;
        os) env="WXTK_OPT=Os"; W="$C/library-build-os" ;;
        baserel) env="WXTK_OPT=Os WXTK_BASEREL=1"; W="$C/library-build-os-baserel" ;;
    esac
    echo "$v: building in $W ($left s left)"
    # shellcheck disable=SC2086
    # a modest -j: KiCad builds on the same machine
    env $env JOBS=6 WORK="$W" OUT="$W/out" nice -n 19 timeout "$left" sh "$HERE/build-amiga.sh" > "$OUTD/build-$v.log" 2>&1
    rc=$?
    echo "$v: exit $rc; gui objects $(ls "$W/gui/obj" 2>/dev/null | grep -c '\.o$')"
    grep -v -e 'warning' -e '^ *|' -e '^ *[0-9]* |' -e 'In file included' -e '^ *from ' "$OUTD/build-$v.log" | cut -c1-300 | tail -n 25
    [ $rc = 0 ] || { : > "$OUTD/exit-$v-$rc"; break; }
    figures "$v" "$W"; echo "$SHA" > "$F/$v/.done"
    cat "$F/$v/sizes.txt"
done
cat "$F/before/sizes.txt" 2>/dev/null | head -40
: > "$OUTD/exit-0"
exit 0
