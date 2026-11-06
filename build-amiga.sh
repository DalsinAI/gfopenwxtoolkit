#!/bin/sh
# Builds GFOpenWX Toolkit for AmigaOS 3, 68040 soft float, with a stove:
# wxBase and its tests, the GFOpenLayout UI library, the GUI library, wx's
# minimal sample and the SDK (include/, lib/, bin/wx-config) apps build with.
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh
#   (OLUI_SRC=... OGT=... to use trees already on disk)
# wxWidgets and our own GFOpenLayout and GFOpenGadTools are fetched at the
# versions wx.lock and deps.lock give; nothing third-party is kept here.
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/deps.lock"
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
# the build tree lives outside the checkout: Cradle cleans the checkout
# (git clean) before each job, so a tree inside it never carries over
WORK="${WORK:-$HOME/.cache/gfopenwxtoolkit/build}"; OUT="${OUT:-$WORK/out}"; JOBS=${JOBS:-12}
mkdir -p "$WORK/deps" "$OUT"

# a repository at one commit, fetched once
fetch() {
    d="$WORK/deps/$1"
    if [ "$(git -C "$d" rev-parse HEAD 2>/dev/null)" != "$3" ]; then
        rm -rf "${d:?}"; mkdir -p "$d"
        git -C "$d" init -q
        git -C "$d" fetch -q --depth 1 "$2" "$3"
        git -C "$d" checkout -q FETCH_HEAD
    fi
}
# OLUI_SRC (a gfopenlayout tree) and OGT (amicradle-guest's
# libraries/opengadtools) can be given instead, for a build without fetching
if [ -z "${OLUI_SRC:-}" ]; then
    fetch gfopenlayout "$OLUI_REPO" "$OLUI_COMMIT"; OLUI_SRC="$WORK/deps/gfopenlayout"
fi
if [ -z "${OGT:-}" ]; then
    fetch amicradle-guest "$OGT_REPO" "$OGT_COMMIT"; OGT="$WORK/deps/amicradle-guest/libraries/opengadtools"
fi

TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" JOBS=$JOBS sh "$HERE/scripts/build-base.sh"

# GFOpenLayout UI (libolui.a), as gfopenlayout's build-ui-amiga.sh builds it
CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
U="$WORK/olui"; mkdir -p "$U/obj"
F="-m68040 -specs=$HERE/scripts/m68k-soft-m68040.specs -noixemul -O2 -fno-delete-null-pointer-checks -fno-late-combine-instructions -std=gnu99 -I$OLUI_SRC/include -I$OLUI_SRC/ui -I$OGT/lib"
for f in src/openlayout.c src/gfcss_parse.c src/gfcss_values.c src/gfcss_select.c src/openlayout_css.c \
         ui/olui.c ui/olui_css.c ui/olui_markup.c ui/olui_theme.c amiga/olui_amiga.c; do
    "$CC" $F -c "$OLUI_SRC/$f" -o "$U/obj/$(basename "$f" .c).o"
done
"$CC" $F -Wno-sign-compare -c "$OGT/lib/ogt_theme.c" -o "$U/obj/ogt_theme.o"
rm -f "$U/libolui.a"; "$STOVE/prefix/bin/m68k-amigaos-ar" rcs "$U/libolui.a" "$U"/obj/*.o

TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" JOBS=$JOBS OLUI="$OLUI_SRC/include" OLUI_LIB="$U/libolui.a" \
    sh "$HERE/scripts/build-gui.sh" 2>&1 | grep -v 'duplicate section' | tail -40
# openwxtoolkit.library, its wx module and the glue programs link with it
TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" sh "$HERE/scripts/build-library.sh"
ls -l "$OUT"
