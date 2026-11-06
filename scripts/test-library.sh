#!/bin/sh
# Proves openwxtoolkit.library's shared code on a small C++ library
# (tests/library) before wx: builds it -fbaserel32 into a module of its own
# with scripts/build-library.sh, a program on it, and the same program
# linked statically to compare. OUT gets Libs/ and shapetest(-static).
#   TOOLCHAIN=... WORK=... OUT=... scripts/test-library.sh
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK=${WORK:-"$HERE/build/libtest"}; OUT=${OUT:-"$WORK/out"}
[ -n "${TOOLCHAIN:-}" ] && PATH="$TOOLCHAIN/bin:$PATH"
SPECS="$HERE/scripts/m68k-soft-m68040.specs"
T="$HERE/tests/library"; mkdir -p "$WORK/t" "$OUT"
CXF="-noixemul -m68040 -O2 -std=gnu++17 -specs=$SPECS"
m68k-amigaos-g++ $CXF -fbaserel32 -fdata-sections -c "$T/shapes.cpp" -o "$WORK/t/shapes.o"
rm -f "$WORK/t/libshapes.a"; m68k-amigaos-ar rcs "$WORK/t/libshapes.a" "$WORK/t/shapes.o"
WORK="$WORK" OUT="$OUT" WXTK_ARCHIVES="$WORK/t/libshapes.a" sh "$HERE/scripts/build-library.sh"
L="$WORK/library"
m68k-amigaos-g++ $CXF -ffixed-a4 -c "$T/main.cpp" -o "$WORK/t/main.o"
m68k-amigaos-g++ $CXF -o "$OUT/shapetest" "$WORK/t/main.o" "$L/wxtoolkit-tables.o" "$L/wxtoolkit-glue.o" \
    -Wl,-Map,"$WORK/t/shapetest.map" -lpthread -latomic -lm -lamiga
m68k-amigaos-g++ $CXF -c "$T/shapes.cpp" -o "$WORK/t/shapes-static.o"
m68k-amigaos-g++ $CXF -c "$T/main.cpp" -o "$WORK/t/main-static.o"
m68k-amigaos-g++ $CXF -o "$OUT/shapetest-static" "$WORK/t/main-static.o" "$WORK/t/shapes-static.o" -lpthread -latomic -lm -lamiga
m68k-amigaos-strip "$OUT/shapetest" "$OUT/shapetest-static"
ls -l "$OUT/shapetest" "$OUT/shapetest-static" "$OUT/Libs/openwxtoolkit.library" "$OUT/Libs/openwxtoolkit/wx.module"
