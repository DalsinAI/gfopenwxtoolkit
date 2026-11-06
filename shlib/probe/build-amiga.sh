#!/bin/sh
# Temporary: can wx's C++ be built -fbaserel32 and linked -resident32 (code
# once, a data copy per program)? Small jobs only; reads the shlib tree.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORK="$HOME/.cache/gfopenwxtoolkit/library-build"
OUTD="${OUT:-out}"; mkdir -p "$OUTD"; LOG="$OUTD/probe.log"
PATH="$STOVE/prefix/bin:$PATH"
P="$WORK/probe"; rm -rf "$P"; mkdir -p "$P"
{
echo "== multilibs"; m68k-amigaos-g++ -print-multi-lib
for f in -m68040 "-m68040 -fbaserel32" "-m68040 -fbaserel32 -resident32"; do
    echo "== $f"
    for l in libstdc++.a libsupc++.a libgcc.a libgcc_eh.a libnix.a libc.a libm.a libpthread.a libamiga.a libatomic.a ncrt0.o nrcrt0.o nlrcrt0.o nbcrt0.o; do
        printf '%s: %s\n' "$l" "$(m68k-amigaos-g++ -noixemul $f -print-file-name=$l)"
    done
done
cat > "$P/t.cpp" <<'X'
#include <stdexcept>
#include <string>
#include <cstdio>
struct B { virtual ~B() {} virtual int f(int x) { return x + 1; } };
struct D : B { std::string s; int f(int x) override { if (x < 0) throw std::runtime_error("neg"); return x * 2; } };
static D g_d;
int counter = 7;
int (*fp)(int) = 0;
extern "C" int probe(int x)
{
    try { B *b = &g_d; counter += b->f(x); }
    catch (const std::exception &e) { std::printf("%s\n", e.what()); return -1; }
    return counter + (int)typeid(g_d).name()[0];
}
X
echo "== t.cpp -fbaserel32"
m68k-amigaos-g++ -noixemul -m68040 -O2 -fbaserel32 -std=gnu++17 -S "$P/t.cpp" -o "$P/t.s" && grep -c "(a4)" "$P/t.s" && grep -n "a4" "$P/t.s" | head -40
m68k-amigaos-g++ -noixemul -m68040 -O2 -fbaserel32 -std=gnu++17 -c "$P/t.cpp" -o "$P/t.o" && m68k-amigaos-objdump -h -r "$P/t.o" | head -80
echo "== link -resident32 -nostartfiles"
printf 'extern "C" int probe(int); int main(){ return probe(3); }\n' > "$P/m.cpp"
m68k-amigaos-g++ -noixemul -m68040 -O2 -fbaserel32 -resident32 -std=gnu++17 -o "$P/t.prog" "$P/m.cpp" "$P/t.o" -Wl,-Map,"$P/t.map" && ls -l "$P/t.prog"
m68k-amigaos-objdump -h "$P/t.prog" 2>&1 | head -20
echo "== a wx file -fbaserel32"
CXXF=$(sed -n 's/^CXXF = //p' "$WORK/gui/Makefile")
for s in src/common/event.cpp src/common/wincmn.cpp src/common/string.cpp; do
    f="$WORK/wxgui/$s"
    m68k-amigaos-g++ $CXXF -fbaserel32 -c "$f" -o "$P/$(basename "$s" .cpp).o" 2>&1 | grep -v -e warning -e '^ ' -e 'In file' | head -10
    ls -l "$P/$(basename "$s" .cpp).o" 2>&1
done
m68k-amigaos-objdump -h "$P/event.o" 2>&1 | head -40
echo "== libnix set lists in baserel"
m68k-amigaos-nm "$(m68k-amigaos-g++ -noixemul -m68040 -fbaserel32 -print-file-name=libnix.a)" 2>/dev/null | grep -i "a4\|LinkerDB\|__saveds" | head -10
} > "$LOG" 2>&1
echo done
exit 0
