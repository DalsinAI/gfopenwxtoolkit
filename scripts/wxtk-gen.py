#!/usr/bin/env python3
"""wxtk-gen.py: the tables that join a program, openwxtoolkit.library and
the wx module (src/library/wxtoolkit_library.h).

  wxtk-gen.py scan OUTDIR ARCHIVE... [-- EXTRA.o...]
      unpacks the toolkit's archives (in link order), picks the members the
      module is linked from (a member whose strong symbols an earlier one
      already defines is left out, as a static link would never use it: the
      GUI build's copies of wxBase files win), and writes
        members.rsp   the members, for the module's link
        symbols.json  what they define and what they leave undefined
        rtprobe.s     a table naming every symbol the module needs from a
                      program's C library, to link a probe program with
  wxtk-gen.py emit OUTDIR RTPROBE.t
      reads the probe program's symbol table (objdump -t: where each C
      library symbol lives, so code from data) and writes
        mod_tables.s  the module's header, its export table, the jumps and
                      stand-ins for its C library imports, and the heads of
                      libnix's lists (the first object of the module's link)
        app_tables.s  a program's side: a jump for each wx call, a stand-in
                      for each piece of wx data, its C library table, and
                      struct WXTKApp (__wxtk_app)
        exports.txt, imports.txt   the lists, for people
Copyright (c) 2026 AmiCradle Team
"""
import json
import os
import re
import subprocess
import sys

OBJDUMP = os.environ.get("OBJDUMP", "m68k-amigaos-objdump")
AR = os.environ.get("AR", "m68k-amigaos-ar")
MAGIC = 0x5758544B
FORMAT = 1
# symbols that are the module's own business, never exported or imported
PRIVATE = re.compile(r"^(___CTOR_LIST__|___DTOR_LIST__|___INIT_LIST__|___EXIT_LIST__|___LIB_LIST__|"
                     r"__EH_FRAME_BEGINS__|__EH_FRAME_OBJECTS__|__EH_FRAME_BEGIN__|__EH_FRAME_OBJECT__|"
                     r"_wxtk_module_.*|___wxtk_.*)$")
TYPEDATA = re.compile(r"^__ZT[VISTC]")      # vtables, typeinfo and their names: data in .rodata


def fnv(name):
    h = 0x811C9DC5
    for b in name.encode():
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def objdump(path):
    """(sections {name: size}, symbols [(name, value, flags, section)])"""
    out = subprocess.run([OBJDUMP, "-h", "-t", path], capture_output=True, text=True, check=True).stdout
    secs, syms = {}, []
    mode = None
    for line in out.splitlines():
        if line.startswith("Sections:"):
            mode = "h"; continue
        if line.startswith("SYMBOL TABLE:"):
            mode = "t"; continue
        t = line.split()
        if mode == "h" and len(t) >= 7 and t[0].isdigit():
            secs[t[1]] = int(t[2], 16)
        elif mode == "t" and len(t) >= 5 and re.match(r"^[0-9a-f]{8}$", t[0]):
            # value [flags...] section size type name
            name, sec = t[-1], t[-4]
            flags = "".join(t[1:-4])
            syms.append((name, int(t[0], 16), flags, sec))
    return secs, syms


def scan(outdir, archives, extras):
    xdir = os.path.join(outdir, "x")
    members = []
    for i, a in enumerate(archives):
        d = os.path.join(xdir, str(i))
        os.makedirs(d, exist_ok=True)
        names = subprocess.run([AR, "t", a], capture_output=True, text=True, check=True).stdout.split()
        subprocess.run([AR, "x", os.path.abspath(a)], cwd=d, check=True)
        if len(set(names)) != len(names):
            sys.exit("wxtk-gen: %s has members of the same name" % a)
        members += [os.path.join(d, n) for n in names]
    members += extras
    taken = set()
    chosen, skipped = [], []
    defs = {}          # name -> {kind, weak, size}
    undef = set()
    lists = set()
    for m in members:
        secs, syms = objdump(m)
        strong = {s[0] for s in syms if "g" in s[2] and s[3] not in ("*UND*", "*ABS*")}
        if strong & taken and m not in extras:
            skipped.append(os.path.basename(m))
            continue
        taken |= strong
        chosen.append(m)
        for sec in secs:
            if sec.startswith(".list___INIT") or sec.startswith(".list___EXIT") or sec.startswith(".dlist___LIB"):
                lists.add(sec)
        # sizes of data symbols: to the next symbol in the section, or its end
        bysec = {}
        for name, val, flags, sec in syms:
            if sec not in ("*UND*", "*ABS*", "*COM*"):
                bysec.setdefault(sec, []).append(val)
        for v in bysec.values():
            v.sort()
        for name, val, flags, sec in syms:
            if sec == "*UND*":
                undef.add(name)
                continue
            if sec == "*ABS*" or not ("g" in flags or "w" in flags) or PRIVATE.match(name):
                continue
            if sec == "*COM*":
                kind, size = "data", val
            else:
                code = sec.startswith(".text") and not TYPEDATA.match(name)
                kind = "code" if code else "data"
                later = [x for x in bysec[sec] if x > val]
                size = (min(later) if later else secs.get(sec, val + 4)) - val
            weak = "w" in flags and "g" not in flags
            old = defs.get(name)
            if old and not old["weak"]:
                continue
            defs[name] = {"kind": kind, "weak": weak, "size": max(size, 2)}
    imports = sorted(n for n in undef if n not in defs and not PRIVATE.match(n))
    with open(os.path.join(outdir, "members.rsp"), "w") as f:
        f.write("\n".join(chosen) + "\n")
    with open(os.path.join(outdir, "symbols.json"), "w") as f:
        json.dump({"exports": defs, "imports": imports}, f)
    with open(os.path.join(outdir, "rtprobe.s"), "w") as f:
        f.write("\t.text\n\t.globl _wxtk_rtprobe\n_wxtk_rtprobe:\n")
        for n in imports:
            f.write("\t.long %s\n" % n)
    nweak = sum(1 for d in defs.values() if d["weak"])
    ncode = sum(1 for d in defs.values() if d["kind"] == "code")
    print("wxtk-gen: %d members (%d left out: %s), %d exports (%d code, %d data, %d weak), %d imports"
          % (len(chosen), len(skipped), " ".join(skipped), len(defs), ncode, len(defs) - ncode, nweak, len(imports)))
    if lists:
        print("wxtk-gen: warning: members carry libnix lists the module doesn't run: %s" % " ".join(sorted(lists)))


def emit(outdir, rtprobe_t):
    sym = json.load(open(os.path.join(outdir, "symbols.json")))
    defs, imports = sym["exports"], sym["imports"]
    # where the probe program's C library put each import
    _, syms = objdump(rtprobe_t) if not rtprobe_t.endswith(".t") else (None, parse_t(rtprobe_t))
    where = {}
    bysec = {}
    for name, val, flags, sec in syms:
        if sec in ("*UND*", "*ABS*"):
            continue
        where.setdefault(name, (sec, val))
        bysec.setdefault(sec, []).append(val)
    for v in bysec.values():
        v.sort()
    impc, impd = [], []
    for n in imports:
        if n not in where:
            sys.exit("wxtk-gen: the probe program doesn't define %s" % n)
        sec, val = where[n]
        if sec.startswith(".text") and not TYPEDATA.match(n):
            impc.append(n)
        else:
            later = [x for x in bysec[sec] if x > val]
            size = (min(later) - val) if later else 4096
            impd.append((n, min(max(size + 16, 16), 65536)))

    exports = sorted(defs, key=fnv)
    hashes = [fnv(n) for n in exports]
    for a, b, n in zip(hashes, hashes[1:], exports[1:]):
        if a == b:
            sys.exit("wxtk-gen: two exports share a hash: %s" % n)
    rth = sorted(set(fnv(n) for n in imports))
    if len(rth) != len(imports):
        sys.exit("wxtk-gen: two imports share a hash")
    buildid = fnv("\n".join(exports) + "\n--\n" + "\n".join(imports))

    # ---- the module's tables: the first object of its link -----------------
    m = []
    w = m.append
    w("| wx.module's tables (wxtk-gen.py)\n\t.text\n\t.globl _wxtk_module_start\n_wxtk_module_start:\n")
    w("\tmoveq #-1,d0\n\trts\n")
    w("\t.long 0x%08x,%d,0x%08x\n" % (MAGIC, FORMAT, buildid))
    w("\t.long %d,Lexph,Lexpa\n" % len(exports))
    w("\t.long %d,Limpc,Limpch\n" % len(impc))
    w("\t.long %d,Limpd,Limpde,Limpdo,Limpdh\n" % len(impd))
    w("\t.long _wxtk_module_init,_wxtk_module_exit,Lnames\n")
    w("\t.align 2\nLexph:\n")
    for h in hashes:
        w("\t.long 0x%08x\n" % h)
    w("Lexpa:\n")
    for n in exports:
        w("\t.long %s\n" % n)
    w("Limpch:\n")
    for n in impc:
        w("\t.long 0x%08x\n" % fnv(n))
    w("Limpdh:\n")
    for n, _ in impd:
        w("\t.long 0x%08x\n" % fnv(n))
    w("Limpdo:\n")
    off = 0
    for n, size in impd:
        w("\t.long %d\n" % off)
        off += size
    w("Limpc:\n")
    for n in impc:
        w("\t.globl %s\n%s:\n\t.short 0x4ef9\n\t.long 0\n" % (n, n))
    w("Lnames:\n")
    for n in impc + [x for x, _ in impd]:
        w("\t.asciz \"%s\"\n" % n)
    w("\t.align 2\n\t.bss\n\t.align 2\nLimpd:\n")
    for n, size in impd:
        w("\t.globl %s\n%s:\t.skip %d\n" % (n, n, size))
    w("Limpde:\t.skip 4\n")
    # libnix's lists: a zero head each, the members' entries, then the end
    # markers (as gfopenrtg's module_start.S): C++ constructors and
    # destructors, and the EH frames each object registers
    for sec, name in ((".list___CTOR_LIST__", "___CTOR_LIST__"), (".list___DTOR_LIST__", "___DTOR_LIST__"),
                      (".list___INIT_LIST__", "___INIT_LIST__"), (".list___EXIT_LIST__", "___EXIT_LIST__"),
                      (".list___EH_FRAME_BEGINS__", "__EH_FRAME_BEGINS__"),
                      (".dlist___EH_FRAME_OBJECTS__", "__EH_FRAME_OBJECTS__"),
                      (".dlist___LIB_LIST__", "___LIB_LIST__")):
        w("\t.section %s,\"aw\"\n\t.globl %s\n%s:\n\t.long 0\n" % (sec, name, name))
    w("\t.section .end_of_lists,\"aw\"\n\t.long 0\n\t.section .end_of_dlists,\"aw\"\n\t.long 0\n")
    open(os.path.join(outdir, "mod_tables.s"), "w").write("".join(m))

    # ---- a program's side --------------------------------------------------
    code = [n for n in exports if defs[n]["kind"] == "code"]
    data = sorted((n for n in exports if defs[n]["kind"] == "data"), key=fnv)
    a = []
    w = a.append
    w("| a program's side of openwxtoolkit.library (wxtk-gen.py)\n\t.text\n\t.align 2\n")
    # a wx call: a jump the library points at the module. Weak, so the
    # program's own copy of an inline function or template (or its own zlib)
    # is the one it uses.
    w("Lstubs:\n")
    for n in code:
        w("\t.weak %s\n%s:\n\t.short 0x4ef9\n\t.long 0\n" % (n, n))
    w("\t.align 2\nLcodeh:\n")
    for n in code:
        w("\t.long 0x%08x\n" % fnv(n))
    w("Lphoff:\n")
    off = 0
    places = []
    for n in data:
        places.append((n, off))
        w("\t.long %d\n" % off)
        off += (defs[n]["size"] + 4 + 3) & ~3
    w("Lphh:\n")
    for n in data:
        w("\t.long 0x%08x\n" % fnv(n))
    # the C library the module uses, sorted by hash; naming each links it in
    rt = sorted(imports, key=fnv)
    w("Lrth:\n")
    for n in rt:
        w("\t.long 0x%08x\n" % fnv(n))
    w("Lrta:\n")
    for n in rt:
        w("\t.long %s\n" % n)
    # wx data: stand-ins. A piece of data wx defines weak (a template's
    # static member, an inline function's static, a vtable or typeinfo
    # emitted where used) is strong here, so the program's copies give way
    # to the module's one; the rest is weak, as the jumps are.
    w("\t.bss\n\t.align 2\nLph:\n")
    for n, o in places:
        w("\t%s %s\n%s:\t.skip %d\n" % (".globl" if defs[n]["weak"] else ".weak", n, n,
                                          (defs[n]["size"] + 4 + 3) & ~3))
    w("Lphe:\t.skip 4\n")
    w("\t.data\n\t.align 2\n\t.globl ___wxtk_app\n___wxtk_app:\n")
    w("\t.long 0x%08x,%d,0x%08x\n" % (MAGIC, FORMAT, buildid))
    w("\t.long %d,Lstubs,Lcodeh\n" % len(code))
    w("\t.long %d,Lph,Lphe,Lphoff,Lphh\n" % len(data))
    w("\t.long %d,Lrth,Lrta\n" % len(rt))
    w("\t.long 0,0,0\n")                      # seglist, dirlock, name
    w("\t.long 0,0,0\n")                      # module, header, fixed
    w("\t.skip 200\n")                        # error
    open(os.path.join(outdir, "app_tables.s"), "w").write("".join(a))
    with open(os.path.join(outdir, "exports.txt"), "w") as f:
        for n in exports:
            f.write("%08x %s %s%s %d\n" % (fnv(n), n, defs[n]["kind"], " weak" if defs[n]["weak"] else "", defs[n]["size"]))
    with open(os.path.join(outdir, "imports.txt"), "w") as f:
        for n in impc:
            f.write("%08x %s code\n" % (fnv(n), n))
        for n, s in impd:
            f.write("%08x %s data %d\n" % (fnv(n), n, s))
    print("wxtk-gen: build id %08x; program side: %d calls, %d data (%d bytes of stand-ins), %d C library symbols; "
          "module imports %d calls, %d data" % (buildid, len(code), len(data), off, len(rt), len(impc), len(impd)))


def parse_t(path):
    syms = []
    for line in open(path):
        t = line.split()
        if len(t) >= 5 and re.match(r"^[0-9a-f]{8}$", t[0]):
            syms.append((t[-1], int(t[0], 16), "".join(t[1:-4]), t[-4]))
    return syms


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "scan":
        rest = sys.argv[3:]
        extras = []
        if "--" in rest:
            k = rest.index("--")
            rest, extras = rest[:k], rest[k + 1:]
        scan(sys.argv[2], rest, extras)
    elif len(sys.argv) == 4 and sys.argv[1] == "emit":
        emit(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
