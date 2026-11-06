#!/bin/sh
# Copyright (c) 2026 AmiCradle Team
# Builds wxBase (wxWidgets' non-GUI libraries) for AmigaOS 3, 68040 soft
# float, and the wxBase test program.
#   TOOLCHAIN  folder with bin/m68k-amigaos-g++ (default: $STOVE/prefix)
#   NDK        extra include folder for the NDK (empty: the compiler's own)
#   WORK       where wxWidgets is unpacked and built (default build/)
#   OUT        where the test program goes (default build/out)
#   JOBS       make -j (default 2)
#   REGEX_SRC  a gfopenregex tree: wxRegEx is regex.library's (its <regex.h> and stub)
#   OXP_SRC    gfopenxml's oxp/: wxXmlDocument's parser, behind src/amiga/expat
#   WXTK_OPT   Os (default: -Os and wxDEBUG_LEVEL=0) or O2 (wx's debug checks kept)
#   WXTK_BASEREL  1: -fbaserel32 -fdata-sections, for the wx module (build-library.sh)
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
REGEX_SRC=${REGEX_SRC:?a gfopenregex tree (build-amiga.sh fetches it)}
OXP_SRC=${OXP_SRC:?the oxp folder of a gfopenxml tree (build-amiga.sh fetches it)}
case "${WXTK_OPT:-Os}" in
    O2) OPT="-O2" ;;
    # the size switch: wx's debug checks and their messages go too
    *) OPT="-Os -DwxDEBUG_LEVEL=0" ;;
esac
BASEREL=""; [ "${WXTK_BASEREL:-0}" = 1 ] && BASEREL="-fbaserel32 -fdata-sections"
# regex.library's <regex.h>, and wx's regex.cpp on it in place of the PCRE it carries
FLAGS="-noixemul -m68040 $OPT $BASEREL $LATE -include $COMPAT -I$REGEX_SRC/include -DwxUSE_PCRE=0"

mkdir -p "$WORK" "$OUT"
SRC="$WORK/wxWidgets-$WX_VERSION"
# the tree is made again whenever the patches change
PSUM=$(cat "$HERE"/patches/*.patch | sha256sum | cut -c1-16)
if [ "$(cat "$SRC/.patched" 2>/dev/null)" != "$PSUM" ]; then
    T="$WORK/wxWidgets-$WX_VERSION.tar.bz2"
    [ -f "$T" ] || curl -sSfL -o "$T" "$WX_URL"
    echo "$WX_SHA256  $T" | sha256sum -c - >/dev/null
    # made in a new tree, then only the files that differ go into the old
    # one: the rest keep their times, so make rebuilds only what a patch
    # touched
    NEW="$WORK/wx-new"; rm -rf "$NEW"; mkdir -p "$NEW"
    tar xjf "$T" -C "$NEW"
    for p in "$HERE"/patches/*.patch; do patch -s -d "$NEW/wxWidgets-$WX_VERSION" -p1 < "$p"; done
    if [ -d "$SRC" ]; then
        (cd "$NEW/wxWidgets-$WX_VERSION" && find . -type f) | while read -r f; do
            cmp -s "$NEW/wxWidgets-$WX_VERSION/$f" "$SRC/$f" || { mkdir -p "$(dirname "$SRC/$f")"; cp -p "$NEW/wxWidgets-$WX_VERSION/$f" "$SRC/$f"; touch "$SRC/$f"; echo "patched: $f"; }
        done
        rm -rf "$NEW"
    else
        mv "$NEW/wxWidgets-$WX_VERSION" "$SRC"; rm -rf "$NEW"
    fi
    echo "$PSUM" > "$SRC/.patched"
fi

B="$WORK/base"
# configured again when CONF, the flags, amigacompat.h or the configure patch change
# (of the patches, only the one to configure: the others are sources, which
# make follows). CONF names how it is configured: change it with the
# configure line, so other edits here don't make it all again
CONF="configure-3"
CSUM=$( (echo "$CONF $FLAGS $INC $SPECS"; cat "$COMPAT" "$HERE"/patches/0001-*.patch) | sha256sum | cut -c1-16)
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
        --enable-sockets --disable-ipv6 --disable-ipc --disable-webrequest --without-subdirs > configure.log 2>&1) \
        || { tail -30 "$B/configure.log"; exit 1; }
    # static only: no position-independent code (m68k-amigaos has no GOT)
    sed -i 's/ -fPIC -DPIC//; s/^PIC_FLAG = .*/PIC_FLAG =/' "$B/Makefile"
    # wx's XML code reads our expat.h (Expat's calls, served by OXP), not its copy's
    sed -i "s|-I\$(top_srcdir)/src/expat/expat/lib|-I$HERE/src/amiga/expat|" "$B/Makefile"
    grep -q "$HERE/src/amiga/expat" "$B/Makefile" || { echo "expat include not replaced" >&2; exit 1; }
    echo "$CSUM" > "$B/.configured"
fi
(cd "$B" && nice -n 19 make -j"$JOBS" > make.log 2>&1) || { grep -E 'error' "$B/make.log" | head -40; exit 1; }

# zlib is z.library's (GFOpenZlib), not wx's built-in copy: libwxzlib holds
# only the glue that calls the library, built against wx's zlib.h (whose
# names wx's code was compiled with)
# shellcheck disable=SC2086
m68k-amigaos-gcc $FLAGS -Wall -I"$SRC/src/zlib" $INC -c "$HERE/src/amiga/wxzlib_zlibrary.c" -o "$B/wxzlib_zlibrary.o"
ZLIB=$(ls "$B"/lib/libwxzlib-*.a)
rm -f "$ZLIB"; m68k-amigaos-ar rcs "$ZLIB" "$B/wxzlib_zlibrary.o"

# wxRegEx is regex.library's: libwxregexu holds its link stub, not PCRE
# shellcheck disable=SC2086
m68k-amigaos-gcc $FLAGS -Wall -I"$REGEX_SRC/src" -c "$REGEX_SRC/stub/regex_stub.c" -o "$B/regex_stub.o"
REGEXLIB=$(ls "$B"/lib/libwxregexu-*.a)
rm -f "$REGEXLIB"; m68k-amigaos-ar rcs "$REGEXLIB" "$B/regex_stub.o"

# wxXmlDocument's parser is OXP, behind the Expat calls it makes: libwxexpat
# holds the shim and OXP, not Expat (plain C: without amigacompat.h)
CFL=$(echo "$FLAGS" | sed "s| -include [^ ]*||")
# shellcheck disable=SC2086
m68k-amigaos-gcc $CFL -std=gnu99 -Wall -I"$HERE/src/amiga/expat" -I"$OXP_SRC/include" -c "$HERE/src/amiga/wxexpat_oxp.c" -o "$B/wxexpat_oxp.o"
# shellcheck disable=SC2086
m68k-amigaos-gcc $CFL -std=gnu99 -fno-delete-null-pointer-checks -I"$OXP_SRC/include" -c "$OXP_SRC/src/oxp.c" -o "$B/oxp.o"
EXPATLIB=$(ls "$B"/lib/libwxexpat-*.a)
rm -f "$EXPATLIB"; m68k-amigaos-ar rcs "$EXPATLIB" "$B/wxexpat_oxp.o" "$B/oxp.o"

CXXF="$FLAGS -std=gnu++17 $INC $("$B/wx-config" --cxxflags | sed 's/ -fPIC -DPIC//')"
# shellcheck disable=SC2086
m68k-amigaos-g++ $FLAGS -std=gnu++17 $INC -Wall -c "$HERE/src/amiga/wxamiga_crt.cpp" -o "$B/wxamiga_crt.o"
# shellcheck disable=SC2086
m68k-amigaos-gcc $FLAGS -Wall -c "$HERE/src/amiga/wxamiga_stubs.c" -o "$B/wxamiga_stubs.o"
for t in basetest wctest sockettest ztest pathtest; do
    # shellcheck disable=SC2086
    m68k-amigaos-g++ $CXXF -I"$SRC/src/zlib" "$HERE/tests/base/$t.cpp" "$B/wxamiga_crt.o" "$B/wxamiga_stubs.o" -o "$OUT/$t" \
        -specs="$SPECS" -s $("$B/wx-config" --libs base,xml,net) -lsocket -lpthread -latomic
done
ls -l "$OUT"
