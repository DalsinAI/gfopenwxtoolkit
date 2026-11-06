#!/bin/sh
# Probe for openwxtoolkit.library: toolchain facts (temporary).
# Copyright (c) 2026 AmiCradle Team
set -u
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
OUT=${OUT:-out}; mkdir -p "$OUT"
P="$STOVE/prefix"; B="$P/bin"; export PATH="$B:$PATH"
T=$(mktemp -d)
W="$HOME/.cache/gfopenwxtoolkit/build"
echo "== gcc"; m68k-amigaos-gcc --version | head -1; ls "$B" | tr '\n' ' '; echo
echo "== libnix stabs priorities"; find "$P" -name stabs.h | head -2; grep -rn "ADD2INIT\|ADD2EXIT" "$(find "$P" -name stabs.h | head -1)" | head; 
echo "== libnix init users"; for f in $(find "$P" -path '*libnix*' -name 'libnix.a' | head -1); do m68k-amigaos-nm "$f" 2>/dev/null | grep -i "INIT_LIST\|initcpp\|__init\|EH_FRAME\|register_frame" | sort | uniq -c | head -30; done
echo "== crt"; find "$P" -name '*crt0*.o' | head -20
cat > "$T/e.cpp" <<'X'
#include <cstdio>
#include <stdexcept>
extern int ext_data; extern int ext_arr[]; extern void ext_fn(int);
struct B { virtual ~B(); virtual int f(); }; struct D : B { int f() override; };
int D::f() { return ext_data + ext_arr[3]; }
int g() { try { ext_fn(1); } catch (std::runtime_error &e) { std::puts(e.what()); return 1; } return 0; }
B *mk() { return new D; }
X
m68k-amigaos-g++ -m68040 -O2 -noixemul -S -o - "$T/e.cpp" | grep -v '^\s*\.\(stab\|loc\|file\|cfi\)' | head -120
echo "== eh model"; m68k-amigaos-g++ -m68040 -O2 -noixemul -dM -E -x c++ /dev/null | grep -i "sjlj\|EXCEPT\|DWARF\|__USING" 
cat > "$T/m.cpp" <<'X'
#include <cstdio>
#include <stdexcept>
int ext_data = 5; int ext_arr[8]; void ext_fn(int x) { if (x) throw std::runtime_error("boom"); }
struct B { virtual ~B(); virtual int f(); }; B::~B() {} int B::f() { return 1; }
int g(); B *mk();
struct S { S() { std::printf("ctor\n"); } } s;
int main() { std::printf("%d %d\n", g(), mk()->f()); return 0; }
X
m68k-amigaos-g++ -m68040 -O2 -noixemul -c "$T/e.cpp" -o "$T/e.o"; m68k-amigaos-g++ -m68040 -O2 -noixemul -c "$T/m.cpp" -o "$T/m.o"
echo "== objdump -h e.o"; m68k-amigaos-objdump -h "$T/e.o" | head -30; m68k-amigaos-objdump -r "$T/e.o" | head -40
m68k-amigaos-g++ -m68040 -noixemul -o "$T/a" "$T/m.o" "$T/e.o" -Wl,-Map,"$T/a.map"
echo "== map head"; grep -n "EH_FRAME\|eh_frame\|__INIT_LIST\|frame_info\|___CTOR\|\.bss\|\.data \|\.text " "$T/a.map" | head -40
echo "== hunks"; python3 - "$T/a" <<'X'
import struct,sys
d=open(sys.argv[1],'rb').read(); p=0
def L(): 
    global p; v=struct.unpack('>I',d[p:p+4])[0]; p+=4; return v
assert L()==0x3f3; L(); n=L(); f=L(); l=L(); sizes=[L() for i in range(l-f+1)]; print('hunks',n,[hex(s) for s in sizes])
while p<len(d):
    t=L()&0x3fffffff
    if t in (0x3e9,0x3ea): n=L(); print(hex(t),n*4); p+=n*4
    elif t==0x3eb: print('bss',L()*4)
    elif t in (0x3ec,):
        while True:
            c=L()
            if not c: break
            h=L(); print(' reloc32',c,'->hunk',h); p+=4*c
    elif t in (0x3fc,0x3f7):
        while True:
            c=struct.unpack('>H',d[p:p+2])[0]; p+=2
            if not c: break
            h=struct.unpack('>H',d[p:p+2])[0]; p+=2; print(' reloc32short',c,'->',h); p+=2*c
        if p%4: p+=2
    elif t==0x3f0:
        k=0
        while True:
            c=L()
            if not c: break
            p+=4*c+4; k+=1
        print(' symbols',k)
    elif t==0x3f1: n=L(); p+=n*4; print(' debug',n*4)
    elif t==0x3f2: print(' end')
    else: print('other',hex(t)); break
X
echo "== existing build tree"; ls -la "$W" "$W/gui" "$W/base/lib" 2>&1 | head -40
if [ -f "$W/gui/libwx_openlayoutu_core.a" ]; then
  L="$W/gui/libwx_openlayoutu_core.a $W/olui/libolui.a $W/base/lib/libwx_baseu-3.2-m68k-amigaos.a $W/base/lib/libwx_baseu_xml-3.2-m68k-amigaos.a $W/base/lib/libwx_baseu_net-3.2-m68k-amigaos.a $W/gui/libwxpng.a $W/gui/libwxjpeg.a $W/gui/libwxscintilla.a $W/base/lib/libwxregexu-3.2-m68k-amigaos.a $W/base/lib/libwxzlib-3.2-m68k-amigaos.a $W/base/lib/libwxexpat-3.2-m68k-amigaos.a"
  m68k-amigaos-ld -r -o "$T/all.o" --whole-archive $L 2>&1 | grep -v 'duplicate section' | head -10
  ls -l "$T/all.o"; m68k-amigaos-size "$T/all.o"
  m68k-amigaos-nm "$T/all.o" > "$T/all.nm"
  echo "undef: $(grep -c ' U ' "$T/all.nm")  defined-global: $(grep -c ' [TDBRVWC] ' "$T/all.nm")"
  awk '{print $(NF-1)}' "$T/all.nm" | sort | uniq -c
  grep ' U ' "$T/all.nm" | awk '{print $2}' | sort -u > "$OUT/undef.txt"; wc -l "$OUT/undef.txt"; head -400 "$OUT/undef.txt" | tr '\n' ' '; echo
  grep ' [DBC] ' "$T/all.nm" | head -5
  m68k-amigaos-nm -S --size-sort "$T/all.o" 2>/dev/null | grep ' [DBdb] ' | tail -5
  cp "$T/all.nm" "$OUT/all.nm"; gzip -f "$OUT/all.nm"
fi
cp "$T/a.map" "$OUT/a.map"; m68k-amigaos-objdump -d "$T/e.o" > "$OUT/e.dis" 2>&1
rm -rf "$T"
exit 0
