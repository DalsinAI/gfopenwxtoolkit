#!/bin/sh
# Builds gfopenwx's GUI library (the openlayout port with wx's common GUI
# code) and wx's minimal sample, after scripts/build-base.sh.
#   TOOLCHAIN=... NDK=... OLUI=gfopenlayout/include OLUI_LIB=libgfopenlayout.a \
#   WORK=build OUT=out JOBS=4 scripts/build-gui.sh
# Copyright (c) 2026 AmiCradle Team
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
. "$HERE/wx.lock"
WORK=${WORK:-$HERE/build}; OUT=${OUT:-$WORK/out}; JOBS=${JOBS:-4}
[ -n "$TOOLCHAIN" ] && PATH="$TOOLCHAIN/bin:$PATH"
SRC="$WORK/wxWidgets-$WX_VERSION"
BASESETUP=$(ls "$WORK"/base/lib/wx/include/*/wx/setup.h)
G="$WORK/gui"; T="$WORK/wxgui"
mkdir -p "$G/wx" "$G/obj" "$OUT"

# setup.h, and wx's tree with the toolkit wired in, made again when what
# goes into them changes (and the objects with them)
python3 "$HERE/scripts/make-setup.py" "$BASESETUP" "$HERE/config/gui-features.txt" "$G/wx/setup.h.new"
cmp -s "$G/wx/setup.h.new" "$G/wx/setup.h" || mv "$G/wx/setup.h.new" "$G/wx/setup.h"
rm -f "$G/wx/setup.h.new"
SUM=$( (cat "$HERE/scripts/add-toolkit.py" "$HERE/config/toolkit-edits.txt" "$SRC/.patched"; ls "$HERE/include/wx/openlayout") | sha256sum | cut -c1-16)
if [ "$(cat "$T/.toolkit" 2>/dev/null)" != "$SUM" ]; then
    rm -rf "$T" "$G/obj"; mkdir -p "$G/obj"
    cp -a "$SRC" "$T"
    python3 "$HERE/scripts/add-toolkit.py" "$T" "$HERE"
    echo "$SUM" > "$T/.toolkit"
fi

# the sources: wx's common GUI code, html, aui, xrc, propgrid, richtext,
# stc, the generic picks, the port, PNG and JPEG (and Scintilla, which stc
# is built on, as its own library)
python3 - "$T/build/files" "$HERE/config/gui-sources.txt" > "$G/sources.txt" <<'PY'
import re, sys
files = open(sys.argv[1]).read()
out = []
for name in ("BASE_AND_GUI_CMN_SRC", "GUI_CMN_SRC", "HTML_CMN_SRC", "AUI_CMN_SRC", "XRC_SRC",
             "PROPGRID_SRC", "RICHTEXT_SRC", "STC_CMN_SRC"):
    m = re.search(r'^' + name + r' =\n((?:[ \t]+.*\n)+)', files, re.M)
    for line in m.group(1).splitlines():
        line = line.split('#', 1)[0]
        out += [w for w in line.split() if not w.startswith('$')]
for line in open(sys.argv[2]):
    line = line.split('#', 1)[0].strip()
    if line.startswith('-'):
        out = [w for w in out if w != line[1:].strip()]
    elif line and line not in out:
        out.append(line)
print('\n'.join(out))
PY

PNG="png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset pngtrans pngwio pngwrite pngwtran pngwutil"
JPEG=$(grep -o 'wxjpeg_[a-z0-9]*\.o' "$T/Makefile.in" | sort -u | sed 's/wxjpeg_//; s/\.o$//')

LATE=""
if m68k-amigaos-gcc -fno-late-combine-instructions -S -o /dev/null -x c /dev/null 2>/dev/null; then LATE="-fno-late-combine-instructions"; fi
CF="-noixemul -m68040 -O2 $LATE"
INC="-I$G -I$HERE/include -I$T/include -I$OLUI ${NDK:+-I$NDK}"
CXXF="$CF -std=gnu++17 -include $HERE/include/wx/openlayout/amigacompat.h -D__WXOPENLAYOUT__ -DWXBUILDING -DwxUSE_GUI=1 $INC -I$T/src/png -I$T/src/jpeg -I$T/src/zlib -Wno-deprecated-declarations"

# one Makefile, so make runs JOBS at once and follows the headers (-MMD)
M="$G/Makefile"
{
    echo "CXXF = $CXXF"
    echo "PNGF = $CF -DNDEBUG -I$T/src/zlib -I$T/src/png -include $HERE/include/wx/openlayout/amigacompat.h"
    echo "JPEGF = $CF -DNDEBUG -I$T/src/jpeg -I$G"
    SCI="-I$T/src/stc/scintilla/include -I$T/src/stc/scintilla/lexlib -I$T/src/stc/scintilla/src -D__WX__ -DSCI_LEXER -DNO_CXX11_REGEX -DLINK_LEXERS"
    echo "STCF = \$(CXXF) $SCI"
    echo "SCIF = \$(CXXF) -DNDEBUG $SCI"
    OBJS=""
    while read -r s; do
        o="obj/$(echo "$s" | sed 's|^src/||; s|/|_|g; s|\.cpp$|.o|')"
        OBJS="$OBJS $o"
        case "$s" in src/stc/*|*/xh_styledtextctrl.cpp) f=STCF ;; *) f=CXXF ;; esac
        printf '%s: %s\n\tm68k-amigaos-g++ $(%s) -MMD -c %s -o $@\n' "$o" "$T/$s" "$f" "$T/$s"
    done < "$G/sources.txt"
    SOBJS=""
    for s in $(grep -oE '^wxscintilla_[A-Za-z0-9_]+\.o: \$\(srcdir\)/[^ ]+' "$T/Makefile.in" | sed 's|.*(srcdir)/||'); do
        o="obj/sci_$(basename "$s" .cxx).o"
        SOBJS="$SOBJS $o"
        printf '%s: %s\n\tm68k-amigaos-g++ $(SCIF) -MMD -c %s -o $@\n' "$o" "$T/$s" "$T/$s"
    done
    for s in "$HERE"/src/openlayout/*.cpp; do
        [ -f "$s" ] || continue
        o="obj/ol_$(basename "$s" .cpp).o"
        OBJS="$OBJS $o"
        printf '%s: %s\n\tm68k-amigaos-g++ $(CXXF) -MMD -c %s -o $@\n' "$o" "$s" "$s"
    done
    POBJS=""; for p in $PNG; do POBJS="$POBJS obj/png_$p.o"; printf 'obj/png_%s.o: %s\n\tm68k-amigaos-gcc $(PNGF) -c %s -o $@\n' "$p" "$T/src/png/$p.c" "$T/src/png/$p.c"; done
    JOBJS=""; for p in $JPEG; do JOBJS="$JOBJS obj/jpeg_$p.o"; printf 'obj/jpeg_%s.o: %s\n\tm68k-amigaos-gcc $(JPEGF) -c %s -o $@\n' "$p" "$T/src/jpeg/$p.c" "$T/src/jpeg/$p.c"; done
    # the Makefile is a prerequisite, so a file taken out of the lists
    # leaves the archive too
    echo "libwx_openlayoutu_core.a: Makefile$OBJS"
    printf '\trm -f $@; m68k-amigaos-ar rcs $@ $(filter %%.o,$^)\n'
    echo "libwxpng.a:$POBJS"
    printf '\trm -f $@; m68k-amigaos-ar rcs $@ $^\n'
    echo "libwxjpeg.a:$JOBJS"
    printf '\trm -f $@; m68k-amigaos-ar rcs $@ $^\n'
    echo "libwxscintilla.a:$SOBJS"
    printf '\trm -f $@; m68k-amigaos-ar rcs $@ $^\n'
    echo "all: libwx_openlayoutu_core.a libwxpng.a libwxjpeg.a libwxscintilla.a"
    echo "-include obj/*.d"
} > "$M"
(cd "$G" && nice -n 19 make -k -j"$JOBS" all)

# wx's minimal sample
L="$WORK/base/lib"
LIBS="$G/libwx_openlayoutu_core.a $OLUI_LIB $L/libwx_baseu-3.2-m68k-amigaos.a $G/libwxpng.a $G/libwxjpeg.a $G/libwxscintilla.a $L/libwxregexu-3.2-m68k-amigaos.a $L/libwxzlib-3.2-m68k-amigaos.a $L/libwxexpat-3.2-m68k-amigaos.a"
m68k-amigaos-g++ $CXXF -c "$T/samples/minimal/minimal.cpp" -I"$T/samples" -o "$G/obj/minimal.o"
m68k-amigaos-g++ -noixemul -m68040 -specs="$HERE/scripts/m68k-soft-m68040.specs" -o "$OUT/minimal" "$G/obj/minimal.o" "$WORK/base/wxamiga_crt.o" "$WORK/base/wxamiga_stubs.o" \
    -Wl,--start-group $LIBS -Wl,--end-group -lsocket -lpthread -latomic -lm -lamiga
m68k-amigaos-strip "$OUT/minimal"
ls -l "$OUT/minimal"

# the SDK a program builds against: wx's headers with the port's, setup.h,
# the libraries under the names wx-config gives, and wx-config itself
SDK="$OUT/sdk"; R=3.2
rm -rf "$SDK"; mkdir -p "$SDK/bin" "$SDK/include/wx-$R" "$SDK/lib/wx/include/openlayout-unicode-static-$R/wx"
cp -a "$T/include/wx" "$SDK/include/wx-$R/"
cp -a "$HERE/include/wx/." "$SDK/include/wx-$R/wx/"
cp "$G/wx/setup.h" "$SDK/lib/wx/include/openlayout-unicode-static-$R/wx/setup.h"
cp "$G/libwx_openlayoutu_core.a" "$SDK/lib/libwx_openlayoutu_core-$R.a"
cp "$G/libwxpng.a" "$SDK/lib/libwxpng-$R.a"
cp "$G/libwxjpeg.a" "$SDK/lib/libwxjpeg-$R.a"
cp "$G/libwxscintilla.a" "$SDK/lib/libwxscintilla-$R.a"
cp "$OLUI_LIB" "$SDK/lib/libgfopenlayout_ui.a"
for l in wx_baseu wx_baseu_xml wx_baseu_net wxregexu wxzlib wxexpat; do
    cp "$L/lib$l-$R-m68k-amigaos.a" "$SDK/lib/lib$l-$R.a"
done
rm -f "$SDK/lib/libwxamiga-$R.a"
m68k-amigaos-ar rcs "$SDK/lib/libwxamiga-$R.a" "$WORK/base/wxamiga_crt.o" "$WORK/base/wxamiga_stubs.o"
cp "$HERE/scripts/wx-config" "$SDK/bin/wx-config"
chmod +x "$SDK/bin/wx-config"

# wx-config's own check: the minimal sample again, built only from the SDK
W="$SDK/bin/wx-config"
# shellcheck disable=SC2046
m68k-amigaos-g++ -noixemul -m68040 -O2 -std=gnu++17 $("$W" --cxxflags) -c "$T/samples/minimal/minimal.cpp" -I"$T/samples" -o "$G/obj/minimal-sdk.o"
# shellcheck disable=SC2046
m68k-amigaos-g++ -noixemul -m68040 -specs="$HERE/scripts/m68k-soft-m68040.specs" -o "$G/minimal-sdk" "$G/obj/minimal-sdk.o" $("$W" --libs core,base,html,adv,qa,xml,net)
ls -l "$G/minimal-sdk"
# the port's own GUI tests, built the same way
for t in "$HERE"/tests/gui/*.cpp; do
    [ -f "$t" ] || continue
    n=$(basename "$t" .cpp)
    # shellcheck disable=SC2046
    m68k-amigaos-g++ -noixemul -m68040 -O2 -std=gnu++17 $("$W" --cxxflags) -c "$t" -o "$G/obj/test-$n.o"
    # shellcheck disable=SC2046
    m68k-amigaos-g++ -noixemul -m68040 -specs="$HERE/scripts/m68k-soft-m68040.specs" -o "$OUT/$n" "$G/obj/test-$n.o" $("$W" --libs core,base)
    m68k-amigaos-strip "$OUT/$n"
done
du -sh "$SDK"
