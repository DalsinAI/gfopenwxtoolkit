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

# the library build first: it is the longest
for v in baserel os o2; do
    # (a variant once built stays: remove its .done to build it again)
    # (the library's programs are linked again every run: the step is quick
    # once the module is made)
    [ -f "$F/$v/.done" ] && [ "$v" != baserel ] && { echo "$v: done already"; continue; }
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
    if [ "$v" = baserel ] && [ -f "$W/out/xrcstart" ] && [ -f "$W/out/sdk/bin/wx-config" ]; then
        # wx is built: only the library, its module and the programs on it
        # (its steps go on where a run cut short stopped)
        env $env WORK="$W" OUT="$W/out" TOOLCHAIN="$STOVE/prefix" nice -n 19 timeout "$left" sh "$HERE/scripts/build-library.sh" > "$OUTD/build-$v.log" 2>&1
    else
    env $env JOBS=6 WORK="$W" OUT="$W/out" nice -n 19 timeout "$left" sh "$HERE/build-amiga.sh" > "$OUTD/build-$v.log" 2>&1
    fi
    rc=$?
    echo "$v: exit $rc; gui objects $(ls "$W/gui/obj" 2>/dev/null | grep -c '\.o$')"
    grep -v -e 'warning' -e '^ *|' -e '^ *[0-9]* |' -e 'In file included' -e '^ *from ' "$OUTD/build-$v.log" | cut -c1-300 | tail -n 25
    [ $rc = 0 ] || { : > "$OUTD/exit-$v-$rc"; break; }
    figures "$v" "$W"; echo "$SHA" > "$F/$v/.done"
    cat "$F/$v/sizes.txt"
done
# (temporary) wctest and basetest again with a trace of how far their start gets
BW="$C/library-build-os-baserel"
if [ -f "$BW/library/wctest.o" ]; then
    mkdir -p "$OUTD/lab"
    m68k-amigaos-gcc -noixemul -m68040 -Os -ffixed-a4 -c "$HERE/shlib/trace/inittrace.c" -o "$BW/library/inittrace.o"
    for t in wctest basetest; do
        # shellcheck disable=SC2046
        WXTK_SHARED=1 m68k-amigaos-g++ -noixemul -m68040 -specs="$HERE/scripts/m68k-soft-m68040.specs" -s -o "$OUTD/lab/$t-tr" \
            "$BW/library/inittrace.o" "$BW/library/$t.o" $(WXTK_SHARED=1 "$BW/out/sdk/bin/wx-config" --libs std) -Wl,-Map,"$OUTD/lab/$t-tr.map" \
            && echo "$t-tr linked"
    done
    m68k-amigaos-objdump -h "$OUTD/lab/wctest-tr" 2>&1 | head -20
    echo "== libnix's cxxglue (init -5, after the glue)"
    NIX=$(m68k-amigaos-gcc -noixemul -m68040 -print-file-name=libnix.a)
    (cd "$BW/library" && rm -rf nixx && mkdir nixx && cd nixx && m68k-amigaos-ar x "$NIX" cxxglue.o && m68k-amigaos-objdump -d -r cxxglue.o | head -150; m68k-amigaos-nm cxxglue.o)
    echo "== wctest-tr's set lists"
    awk '/^ ?\.list___(CTOR|INIT|EXIT|DTOR)_LIST__/{p=1;print;next} p&&/^ ?\.[a-z]/{p=0} p' "$OUTD/lab/wctest-tr.map" | cut -c1-160 | head -150
fi
# the lab's timer: runs a command N times, E-clock ms each
m68k-amigaos-gcc -noixemul -m68040 -Os -specs="$HERE/scripts/m68k-soft-m68040.specs" -s -o "$F/wxtime" "$HERE/tests/amiga/wxtime.c" \
    && echo "wxtime built"
# regex.library for the lab, from the commit deps.lock gives
RX="$C/library-build-os/deps/gfopenregex"
if [ -d "$RX" ] && [ ! -f "$F/regex.library" ]; then
    rm -rf "$C/regex-build"; cp -a "$RX" "$C/regex-build"
    (cd "$C/regex-build" && STOVE="$STOVE" sh build-amiga.sh > "$C/regex-build.log" 2>&1) \
        && cp "$C/regex-build/build-amiga/lib/regex.library" "$F/regex.library" || tail -n 20 "$C/regex-build.log"
fi
# everything the lab needs, as this job's files
mkdir -p "$OUTD/lab"
for v in o2 os baserel; do [ -d "$F/$v" ] && cp -a "$F/$v" "$OUTD/lab/"; done
for f in wxtime regex.library; do [ -f "$F/$f" ] && cp "$F/$f" "$OUTD/lab/"; done
cat "$F"/*/sizes.txt > "$OUTD/lab/sizes.txt" 2>/dev/null
echo "== figures"; ls -l "$F" "$F"/* 2>&1 | head -60
echo "== the ports' tree"; ls -l "$C/build" "$C/build/out" 2>&1 | head -20
cat "$F/before/sizes.txt" 2>/dev/null | head -40
: > "$OUTD/exit-0"
exit 0
