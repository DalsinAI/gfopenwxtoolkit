#!/bin/sh
# Builds GFOpenWX Toolkit for AmigaOS 3, 68040 soft float, with a stove:
# wxBase and its tests, the GFOpenLayout UI library, the GUI library, wx's
# minimal sample and the SDK (include/, lib/, bin/wx-config) apps build with.
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh
#   (OLUI_SRC=... OGT=... REGEX_SRC=... OXP_SRC=... to use trees already on disk)
#   WXTK_OPT=O2      the whole build at -O2, with wx's debug checks (the
#                    default: -Os, -O2 for the hot files, wxDEBUG_LEVEL=0)
#   WXTK_BASEREL=1   wx built -fbaserel32 -fdata-sections, for the wx module
#   (each has a build tree of its own)
# wxWidgets and our own GFOpenLayout and GFOpenGadTools are fetched at the
# versions wx.lock and deps.lock give; nothing third-party is kept here.
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/deps.lock"
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
# the build tree lives outside the checkout: Cradle cleans the checkout
# (git clean) before each job, so a tree inside it never carries over
# (the -O2 tree keeps the name the earlier builds used)
DEFWORK="$HOME/.cache/gfopenwxtoolkit/build"
[ "${WXTK_OPT:-Os}" = O2 ] || DEFWORK="$DEFWORK-os"
[ "${WXTK_BASEREL:-0}" = 1 ] && DEFWORK="$DEFWORK-baserel"
WORK="${WORK:-$DEFWORK}"; OUT="${OUT:-$WORK/out}"; JOBS=${JOBS:-12}
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

if [ -z "${REGEX_SRC:-}" ]; then
    fetch gfopenregex "$REGEX_REPO" "$REGEX_COMMIT"; REGEX_SRC="$WORK/deps/gfopenregex"
fi
if [ -z "${OXP_SRC:-}" ]; then
    fetch gfopenxml "$OXP_REPO" "$OXP_COMMIT"; OXP_SRC="$WORK/deps/gfopenxml/oxp"
fi
export REGEX_SRC OXP_SRC

TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" JOBS=$JOBS sh "$HERE/scripts/build-base.sh"

# GFOpenLayout UI (libolui.a), as gfopenlayout's build-ui-amiga.sh builds it
STOVE="$STOVE" WORK="$WORK" OLUI_SRC="$OLUI_SRC" OGT="$OGT" sh "$HERE/scripts/build-olui.sh"
U="$WORK/olui"

TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" JOBS=$JOBS OLUI="$OLUI_SRC/include" OLUI_LIB="$U/libolui.a" \
    sh "$HERE/scripts/build-gui.sh" 2>&1 | grep -v 'duplicate section' | tail -40
# openwxtoolkit.library, its wx module and the glue programs link with it
# (the module is made of wx built -fbaserel32)
if [ "${WXTK_BASEREL:-0}" = 1 ]; then
    TOOLCHAIN="$STOVE/prefix" WORK="$WORK" OUT="$OUT" sh "$HERE/scripts/build-library.sh"
fi
ls -l "$OUT"
