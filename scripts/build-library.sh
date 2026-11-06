#!/bin/sh
# Builds openwxtoolkit.library, the wx module it loads and a program's link
# glue, from the toolkit's libraries (after build-base.sh and build-gui.sh).
# src/library/wxtoolkit_library.h says how they fit together.
#   TOOLCHAIN=... WORK=... OUT=... scripts/build-library.sh
# OUT gets Libs/openwxtoolkit.library, Libs/openwxtoolkit/wx.module and,
# when OUT/sdk exists, the glue in sdk/lib (wx-config --libs with
# WXTK_SHARED=1 links it); WORK/library keeps the tables and lists.
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$HERE/wx.lock"
WORK=${WORK:-"$HERE/build"}; OUT=${OUT:-"$WORK/out"}
[ -n "${TOOLCHAIN:-}" ] && PATH="$TOOLCHAIN/bin:$PATH"
SPECS="$HERE/scripts/m68k-soft-m68040.specs"
LATE=""
if m68k-amigaos-gcc -fno-late-combine-instructions -S -o /dev/null -x c /dev/null 2>/dev/null; then LATE="-fno-late-combine-instructions"; fi
CF="-noixemul -m68040 -O2 $LATE -fno-delete-null-pointer-checks"
G="$WORK/gui"; BL="$WORK/base/lib"; L="$WORK/library"; R=3.2
rm -rf "$L"; mkdir -p "$L" "$OUT/Libs/openwxtoolkit"

# the module's start and end
m68k-amigaos-gcc $CF -Wall -c "$HERE/src/library/wxtk_module_rt.c" -o "$L/wxtk_module_rt.o"

# the module's members, in the order a static link takes them
ARCH="$G/libwx_openlayoutu_core.a $WORK/olui/libolui.a"
for l in wx_baseu wx_baseu_xml wx_baseu_net; do ARCH="$ARCH $BL/lib$l-$R-m68k-amigaos.a"; done
for l in libwxpng.a libwxjpeg.a libwxscintilla.a; do [ -f "$G/$l" ] && ARCH="$ARCH $G/$l"; done
for l in wxregexu wxzlib wxexpat; do ARCH="$ARCH $BL/lib$l-$R-m68k-amigaos.a"; done
# shellcheck disable=SC2086
python3 "$HERE/scripts/wxtk-gen.py" scan "$L" $ARCH -- "$L/wxtk_module_rt.o"

# which of the C library symbols the module needs are code and which data:
# a program that names them all, as wx-config links one
RTLIBS="$WORK/base/wxamiga_crt.o $WORK/base/wxamiga_stubs.o -lsocket -lpthread -latomic -lm -lamiga"
m68k-amigaos-as -m68040 "$L/rtprobe.s" -o "$L/rtprobe.o"
printf 'int main(void) { return 0; }\n' > "$L/rtmain.c"
m68k-amigaos-gcc $CF -c "$L/rtmain.c" -o "$L/rtmain.o"
# first with what nothing defines allowed: the members that need it are
# left out (wx files for backends this build lacks), then for real
# shellcheck disable=SC2086
m68k-amigaos-g++ -noixemul -m68040 -specs="$SPECS" -o "$L/rtprobe0" "$L/rtmain.o" "$L/rtprobe.o" $RTLIBS \
    -Wl,--unresolved-symbols=ignore-all
m68k-amigaos-objdump -t "$L/rtprobe0" > "$L/rtprobe0.t"
python3 "$HERE/scripts/wxtk-gen.py" prune "$L" "$L/rtprobe0.t"
m68k-amigaos-as -m68040 "$L/rtprobe.s" -o "$L/rtprobe.o"
# shellcheck disable=SC2086
m68k-amigaos-g++ -noixemul -m68040 -specs="$SPECS" -o "$L/rtprobe" "$L/rtmain.o" "$L/rtprobe.o" $RTLIBS
m68k-amigaos-objdump -t "$L/rtprobe" > "$L/rtprobe.t"
python3 "$HERE/scripts/wxtk-gen.py" emit "$L" "$L/rtprobe.t"

# the module: its tables first, then the members; nothing else
m68k-amigaos-as -m68040 "$L/mod_tables.s" -o "$L/mod_tables.o"
m68k-amigaos-gcc -noixemul -m68040 -nostartfiles -nostdlib -o "$L/wx.module.debug" "$L/mod_tables.o" @"$L/members.rsp" \
    -Wl,-Map,"$L/wx.module.map" 2>&1 | grep -v 'duplicate section' || true
[ -f "$L/wx.module.debug" ] || { echo "build-library: the module didn't link" >&2; exit 1; }
und=$(m68k-amigaos-nm -u "$L/wx.module.debug" || true)
[ -z "$und" ] || { echo "build-library: undefined in wx.module: $und" >&2; exit 1; }
m68k-amigaos-strip -o "$OUT/Libs/openwxtoolkit/wx.module" "$L/wx.module.debug"

# the library
m68k-amigaos-gcc -m68040 -O2 -fno-delete-null-pointer-checks -fomit-frame-pointer -fno-toplevel-reorder -fno-builtin \
    -Wall -Wno-unused-parameter -nostartfiles -nostdlib -specs="$SPECS" -I"$HERE/src/library" \
    -o "$L/openwxtoolkit.library" "$HERE/src/library/openwxtoolkit_lib.c" -lgcc
und=$(m68k-amigaos-nm -u "$L/openwxtoolkit.library" || true)
[ -z "$und" ] || { echo "build-library: undefined in openwxtoolkit.library: $und" >&2; exit 1; }
m68k-amigaos-strip -o "$OUT/Libs/openwxtoolkit.library" "$L/openwxtoolkit.library"

# a program's side
m68k-amigaos-as -m68040 "$L/app_tables.s" -o "$L/wxtoolkit-tables.o"
m68k-amigaos-gcc $CF -Wall -I"$HERE/src/library" -c "$HERE/src/library/wxtk_glue.c" -o "$L/wxtoolkit-glue.o"
if [ -d "$OUT/sdk/lib" ]; then
    cp "$L/wxtoolkit-tables.o" "$L/wxtoolkit-glue.o" "$OUT/sdk/lib/"
fi
ls -l "$OUT/Libs/openwxtoolkit.library" "$OUT/Libs/openwxtoolkit/wx.module" "$L/wxtoolkit-tables.o" "$L/wxtoolkit-glue.o"

# programs on the library: wx's minimal sample, the wxBase tests and the
# GUI tests, linked as WXTK_SHARED=1 links any program
if [ -x "$OUT/sdk/bin/wx-config" ]; then
    W="$OUT/sdk/bin/wx-config"; S="$OUT/shared"; mkdir -p "$S"
    APPF="-noixemul -m68040 -O2 $LATE -std=gnu++17"
    build() {   # name source
        # shellcheck disable=SC2046
        m68k-amigaos-g++ $APPF $("$W" --cxxflags) -I"$WORK/wxgui/samples" -c "$2" -o "$L/$1.o"
        # shellcheck disable=SC2046
        WXTK_SHARED=1 m68k-amigaos-g++ -noixemul -m68040 -specs="$SPECS" -o "$S/$1" "$L/$1.o" $(WXTK_SHARED=1 "$W" --libs std)
        m68k-amigaos-strip "$S/$1"
    }
    build minimal "$WORK/wxgui/samples/minimal/minimal.cpp"
    for t in basetest wctest sockettest; do build "$t" "$HERE/tests/base/$t.cpp"; done
    for t in "$HERE"/tests/gui/*.cpp; do [ -f "$t" ] && build "$(basename "$t" .cpp)" "$t"; done
    ls -l "$S"
fi
