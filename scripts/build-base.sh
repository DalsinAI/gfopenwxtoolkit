#!/bin/sh
# Copyright (c) 2026 AmiCradle Team
# Builds wxBase (wxWidgets' non-GUI libraries) for AmigaOS 3, 68040 soft
# float, and the wxBase test program.
#   TOOLCHAIN  folder with bin/m68k-amigaos-g++ (default: $STOVE/prefix)
#   NDK        extra include folder for the NDK (empty: the compiler's own)
#   WORK       where wxWidgets is unpacked and built (default build/)
#   OUT        where the test program goes (default build/out)
#   JOBS       make -j (default 2)
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$HERE/wx.lock"
TOOLCHAIN=${TOOLCHAIN:-"${STOVE:-$HOME/AmiCradle/stoves/os32-gcc16}/prefix"}
NDK=${NDK:-}
WORK=${WORK:-"$HERE/build"}
OUT=${OUT:-"$WORK/out"}
JOBS=${JOBS:-2}
PATH="$TOOLCHAIN/bin:$PATH"; export PATH
SPECS="$HERE/scripts/m68k-soft-m68040.specs"
COMPAT="$HERE/include/wx/openlayout/amigacompat.h"
INC=""; [ -n "$NDK" ] && INC="-I$NDK"
LATE=""
if m68k-amigaos-gcc -fno-late-combine-instructions -S -o /dev/null -x c /dev/null 2>/dev/null; then LATE="-fno-late-combine-instructions"; fi
FLAGS="-noixemul -m68040 -O2 $LATE -include $COMPAT"

mkdir -p "$WORK" "$OUT"
SRC="$WORK/wxWidgets-$WX_VERSION"
# the tree is made again whenever the patches change
PSUM=$(cat "$HERE"/patches/*.patch | sha256sum | cut -c1-16)
if [ "$(cat "$SRC/.patched" 2>/dev/null)" != "$PSUM" ]; then
    T="$WORK/wxWidgets-$WX_VERSION.tar.bz2"
    [ -f "$T" ] || curl -sSfL -o "$T" "$WX_URL"
    echo "$WX_SHA256  $T" | sha256sum -c - >/dev/null
    rm -rf "$SRC"; tar xjf "$T" -C "$WORK"
    for p in "$HERE"/patches/*.patch; do patch -s -d "$SRC" -p1 < "$p"; done
    echo "$PSUM" > "$SRC/.patched"
fi

B="$WORK/base"
# configured again when this script, amigacompat.h or the patches change
CSUM=$( (cat "$0" "$COMPAT"; echo "$PSUM") | sha256sum | cut -c1-16)
if [ "$(cat "$B/.configured" 2>/dev/null)" != "$CSUM" ]; then
    rm -rf "$B"; mkdir -p "$B"
    # libnix's wcsftime is a stub that returns 0: wx then uses strftime.
    # libnix has dlopen (libnix.a), but configure's probe redeclares it and
    # trips over dlfcn.h, which amigacompat.h brings in
    (cd "$B" && ac_cv_func_wcsftime=no ac_cv_func_dlopen=yes ac_cv_func_dlerror=yes CFLAGS="$FLAGS" CXXFLAGS="$FLAGS" CPPFLAGS="$INC" \
        LDFLAGS="-noixemul -m68040 -specs=$SPECS" LIBS="-lsocket -lpthread -latomic" \
        "$SRC/configure" --host=m68k-amigaos --build="$(sh "$SRC/config.guess")" \
        --disable-gui --disable-shared --enable-unicode --disable-sys-libs \
        --with-zlib=builtin --with-regex=builtin --with-expat=builtin \
        --enable-dynlib --disable-dynamicloader --enable-threads --disable-tests \
        --enable-sockets --disable-ipc --disable-webrequest --without-subdirs > configure.log 2>&1) \
        || { tail -30 "$B/configure.log"; exit 1; }
    # static only: no position-independent code (m68k-amigaos has no GOT)
    sed -i 's/ -fPIC -DPIC//; s/^PIC_FLAG = .*/PIC_FLAG =/' "$B/Makefile"
    echo "$CSUM" > "$B/.configured"
fi
(cd "$B" && nice -n 19 make -j"$JOBS" > make.log 2>&1) || { grep -E 'error' "$B/make.log" | head -40; exit 1; }

CXXF="$FLAGS -std=gnu++17 $INC $("$B/wx-config" --cxxflags | sed 's/ -fPIC -DPIC//')"
# shellcheck disable=SC2086
m68k-amigaos-g++ $FLAGS -std=gnu++17 $INC -Wall -c "$HERE/src/amiga/wxamiga_crt.cpp" -o "$B/wxamiga_crt.o"
# shellcheck disable=SC2086
m68k-amigaos-gcc $FLAGS -Wall -c "$HERE/src/amiga/wxamiga_stubs.c" -o "$B/wxamiga_stubs.o"
for t in basetest wctest sockettest; do
    # shellcheck disable=SC2086
    m68k-amigaos-g++ $CXXF "$HERE/tests/base/$t.cpp" "$B/wxamiga_crt.o" -o "$OUT/$t" \
        -specs="$SPECS" $("$B/wx-config" --libs base,xml,net) -lsocket -lpthread -latomic
    m68k-amigaos-strip "$OUT/$t"
done
ls -l "$OUT"
