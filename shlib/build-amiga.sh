#!/bin/sh
# Probe 2 for openwxtoolkit.library: toolchain facts (temporary).
# Copyright (c) 2026 AmiCradle Team
set -u
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
OUT=${OUT:-out}; mkdir -p "$OUT"
P="$STOVE/prefix"; B="$P/bin"; export PATH="$B:$PATH"
T=$(mktemp -d)
W="$HOME/.cache/gfopenwxtoolkit/build"
NIX="$P/m68k-amigaos/libnix/lib/libm020/libnix.a"; [ -f "$NIX" ] || NIX="$P/m68k-amigaos/libnix/lib/libnix.a"
echo "== stabs.h"; cat "$P/m68k-amigaos/sys-include/stabs.h"
mkdir -p "$T/nix"; (cd "$T/nix" && m68k-amigaos-ar x "$NIX" cxxglue.o __initcpp.o __initlibraries.o __initmalloc.o 2>&1)
for o in cxxglue __initcpp; do echo "== $o"; m68k-amigaos-objdump -dr "$T/nix/$o.o" | head -150; m68k-amigaos-objdump -t "$T/nix/$o.o" | head -40; done
echo "== init priorities"; for o in "$T"/nix/*.o; do m68k-amigaos-objdump -s -j .list___INIT_LIST__ "$o" 2>/dev/null | tail -3; done
echo "== ncrt0"; m68k-amigaos-objdump -dr "$P/m68k-amigaos/libnix/lib/ncrt0.o" | head -90
echo "== weak test"
printf '\t.text\n\t.weak _foo\n_foo:\n\tjmp 0x12345678\n\t.globl _bar\n_bar:\n\tjmp 0\n\t.bss\n\t.globl _dat\n_dat:\t.skip 8\n' > "$T/w.s"
printf 'int foo(void){return 7;}\n' > "$T/f.c"
printf 'extern int foo(void); extern int dat; int main(void){return foo()+dat;}\n' > "$T/m.c"
m68k-amigaos-as -m68040 "$T/w.s" -o "$T/w.o" && m68k-amigaos-gcc -m68040 -noixemul -c "$T/f.c" -o "$T/f.o" && m68k-amigaos-gcc -m68040 -noixemul -c "$T/m.c" -o "$T/m.o"
m68k-amigaos-gcc -m68040 -noixemul -o "$T/a" "$T/m.o" "$T/f.o" "$T/w.o" -Wl,-Map,"$T/a.map" && grep -n "_foo\|_dat" "$T/a.map" | head
echo "@file:"; echo "$T/m.o $T/w.o" > "$T/rsp"; m68k-amigaos-ld -r -o "$T/r.o" @"$T/rsp" && echo rsp-ok
m68k-amigaos-nm "$T/w.o"
echo "== wx members"
if [ -f "$W/gui/libwx_openlayoutu_core.a" ]; then
  L="$W/gui/libwx_openlayoutu_core.a $W/olui/libolui.a $W/base/lib/libwx_baseu-3.2-m68k-amigaos.a $W/base/lib/libwx_baseu_xml-3.2-m68k-amigaos.a $W/base/lib/libwx_baseu_net-3.2-m68k-amigaos.a $W/gui/libwxpng.a $W/gui/libwxjpeg.a $W/gui/libwxscintilla.a $W/base/lib/libwxregexu-3.2-m68k-amigaos.a $W/base/lib/libwxzlib-3.2-m68k-amigaos.a $W/base/lib/libwxexpat-3.2-m68k-amigaos.a"
  ls -l $L
  i=0; for a in $L; do i=$((i+1)); mkdir -p "$T/x/$i"; (cd "$T/x/$i" && m68k-amigaos-ar x "$a"); done
  python3 - "$T" <<'X'
import os,subprocess,sys,collections
T=sys.argv[1]; taken=set(); sel=[]; skipped=[]
for i in sorted(os.listdir(T+'/x'),key=int):
    for f in sorted(os.listdir(T+'/x/'+i)):
        p=T+'/x/'+i+'/'+f
        out=subprocess.run(['m68k-amigaos-nm',p],capture_output=True,text=True).stdout
        strong={l.split()[-1] for l in out.splitlines() if len(l.split())==3 and l.split()[1] in 'TDBRC'}
        if strong & taken: skipped.append((p,len(strong&taken),len(strong))); continue
        taken|=strong; sel.append(p)
print('selected',len(sel),'skipped',len(skipped))
for s in skipped[:40]: print(' skip',s)
open(T+'/rsp','w').write(' '.join(sel))
X
  m68k-amigaos-ld -r -o "$T/all.o" @"$T/rsp" 2>&1 | grep -v 'duplicate section' | head -10
  ls -l "$T/all.o"; m68k-amigaos-size "$T/all.o"
  m68k-amigaos-nm "$T/all.o" > "$T/all.nm"
  awk 'NF==3{print $2} NF==2{print $1}' "$T/all.nm" | sort | uniq -c
  grep ' U ' "$T/all.nm" | awk '{print $2}' | sort -u > "$OUT/undef.txt"; wc -l < "$OUT/undef.txt"; tr '\n' ' ' < "$OUT/undef.txt" | head -c 30000; echo
  m68k-amigaos-objdump -h "$T/all.o" | head -60
  cat $(cat "$T/rsp") | wc -c
  gzip -c "$T/all.nm" > "$OUT/all.nm.gz"
  # section of each symbol, from the members
  for f in $(head -c 2000000 "$T/rsp"); do m68k-amigaos-objdump -t "$f" | grep -v '^$' ; done | grep -E '^[0-9a-f]{8} ' | gzip > "$OUT/members-t.gz"
  zcat "$OUT/members-t.gz" | awk '{print $3, $4}' | sort | uniq -c | sort -rn | head -30
fi
rm -rf "$T"
exit 0
