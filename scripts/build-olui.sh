#!/bin/sh
# Builds GFOpenLayout UI (WORK/olui/libolui.a) as gfopenlayout's
# build-ui-amiga.sh builds it, from OLUI_SRC (a gfopenlayout tree) and OGT
# (amicradle-guest's GFOpenGadTools library sources). With WXTK_BASEREL=1 it
# is built -fbaserel32 like the rest of the wx module: code built otherwise
# keeps its own data once for every program and uses A4 as it likes, so the
# module's calls out of it would find A4 off the program's copy.
#   STOVE=... WORK=... OLUI_SRC=... OGT=... scripts/build-olui.sh
# Copyright (c) 2026 AmiCradle Team
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
BASEREL=""; [ "${WXTK_BASEREL:-0}" = 1 ] && BASEREL="-fbaserel32 -fdata-sections"
U="$WORK/olui"; rm -rf "$U/obj"; mkdir -p "$U/obj"
F="-m68040 -specs=$HERE/scripts/m68k-soft-m68040.specs -noixemul -O2 $BASEREL -fno-delete-null-pointer-checks -fno-late-combine-instructions -std=gnu99 -I$OLUI_SRC/include -I$OLUI_SRC/ui -I$OGT/lib"
for f in src/openlayout.c src/gfcss_parse.c src/gfcss_values.c src/gfcss_select.c src/openlayout_css.c \
         ui/olui.c ui/olui_css.c ui/olui_markup.c ui/olui_theme.c amiga/olui_amiga.c; do
    "$CC" $F -c "$OLUI_SRC/$f" -o "$U/obj/$(basename "$f" .c).o"
done
"$CC" $F -Wno-sign-compare -c "$OGT/lib/ogt_theme.c" -o "$U/obj/ogt_theme.o"
rm -f "$U/libolui.a"; "$STOVE/prefix/bin/m68k-amigaos-ar" rcs "$U/libolui.a" "$U"/obj/*.o
