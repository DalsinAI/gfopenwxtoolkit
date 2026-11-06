#!/bin/sh
# Temporary: compile checks for regex.library and OXP in wx, on the stove.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUTD="${OUT:-out}"; mkdir -p "$OUTD"
PATH="$STOVE/prefix/bin:$PATH"
. "$HERE/deps.lock"; . "$HERE/wx.lock"
W="$HOME/.cache/gfopenwxtoolkit/probe"; mkdir -p "$W"
fetch() {
    d="$W/$1"
    if [ "$(git -C "$d" rev-parse HEAD 2>/dev/null)" != "$3" ]; then
        rm -rf "${d:?}"; mkdir -p "$d"; git -C "$d" init -q
        git -C "$d" fetch -q --depth 1 "$2" "$3"; git -C "$d" checkout -q FETCH_HEAD
    fi
}
{
fetch gfopenregex "$REGEX_REPO" "$REGEX_COMMIT"; fetch gfopenxml "$OXP_REPO" "$OXP_COMMIT"
RX="$W/gfopenregex"; OXP="$W/gfopenxml/oxp"
B="$HOME/.cache/gfopenwxtoolkit/build"
COMPAT="$HERE/include/wx/openlayout/amigacompat.h"
for v in "-O2" "-Os -fbaserel32 -fdata-sections"; do
    echo "== $v"
    F="-noixemul -m68040 $v -fno-late-combine-instructions -include $COMPAT -I$RX/include -DwxUSE_PCRE=0 -DwxDEBUG_LEVEL=0"
    m68k-amigaos-gcc $F -Wall -I"$RX/src" -c "$RX/stub/regex_stub.c" -o "$W/regex_stub.o" && echo "regex stub ok"
    m68k-amigaos-gcc $F -std=gnu99 -Wall -I"$HERE/src/amiga/expat" -I"$OXP/include" -c "$HERE/src/amiga/wxexpat_oxp.c" -o "$W/shim.o" && echo "shim ok"
    m68k-amigaos-gcc $F -std=gnu99 -fno-delete-null-pointer-checks -I"$OXP/include" -c "$OXP/src/oxp.c" -o "$W/oxp.o" && echo "oxp ok"
    # wx's regex.cpp with the patch, and xml.cpp on our expat.h, against the -O2 tree's setup.h
    rm -rf "$W/rx"; mkdir -p "$W/rx/src/common"; cp "$B/wxWidgets-$WX_VERSION/src/common/regex.cpp" "$W/rx/src/common/"
    patch -s -d "$W/rx" -p1 < "$HERE/patches/0007-regex-regex-library.patch" && echo "patch ok"
    WXI="-I$B/base/lib/wx/include/$(ls "$B/base/lib/wx/include")" 
    m68k-amigaos-g++ $F -std=gnu++17 $WXI -I"$B/wxWidgets-$WX_VERSION/include" -D__WXBASE__ -DWXBUILDING -DwxUSE_GUI=0 -c "$W/rx/src/common/regex.cpp" -o "$W/regex.o" && echo "regex.cpp ok"
    m68k-amigaos-g++ $F -std=gnu++17 $WXI -I"$B/wxWidgets-$WX_VERSION/include" -I"$HERE/src/amiga/expat" -D__WXBASE__ -DWXBUILDING -DwxUSE_GUI=0 -c "$B/wxWidgets-$WX_VERSION/src/xml/xml.cpp" -o "$W/xml.o" && echo "xml.cpp ok"
    m68k-amigaos-nm -u "$W/regex.o" | grep -i -e reg -e pcre
    m68k-amigaos-size "$W/regex_stub.o" "$W/shim.o" "$W/oxp.o" "$W/regex.o" "$W/xml.o"
done
echo "== old archives (-O2 tree)"
ls -l "$B"/base/lib/libwxregexu-*.a "$B"/base/lib/libwxexpat-*.a "$B"/base/lib/libwxzlib-*.a
m68k-amigaos-size -t "$B"/base/lib/libwxregexu-*.a | tail -1; m68k-amigaos-size -t "$B"/base/lib/libwxexpat-*.a | tail -1
ls -l "$B/out" 2>/dev/null | head -20
} > "$OUTD/probe.log" 2>&1
cat "$OUTD/probe.log" | tail -n 60
: > "$OUTD/exit-0"
exit 0
