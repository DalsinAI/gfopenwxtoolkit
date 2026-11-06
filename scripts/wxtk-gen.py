#!/usr/bin/env python3
"""wxtk-gen.py: the tables that join a program, openwxtoolkit.library and
the wx module (src/library/wxtoolkit_library.h). scripts/build-library.sh
runs it in these steps:

  wxtk-gen.py scan OUTDIR ARCHIVE...
      unpacks the toolkit's archives (built -fbaserel32, in link order) and
      picks the members the module is linked from: a member whose own
      symbols an earlier archive already gives is left out, as a static link
      would never take it (the GUI build's copies of wxBase files win).
      Writes members.rsp (for the links) and members.json.
  wxtk-gen.py link OUTDIR MOD1.t MOD1.log NAMES
      after a first link of the module (the members, the C++ runtime whole,
      no C library): MOD1.t is its symbol table (objdump -t), MOD1.log its
      linker's complaints (--no-demangle), NAMES the global names its inputs
      define (nm), whole: a hunk file cuts long names short. What it defines is what the
      module gives; what it lacks is what it takes from a program's C
      library. Writes state.json and rtprobe.s, a table naming each import,
      to link a probe program with.
  wxtk-gen.py absent OUTDIR PROBE0.log
      after a probe link that may fail: the imports no C library has get a
      stand-in in the module that traps if used (as a static link of a
      program using them would fail), and rtprobe.s is written again.
  wxtk-gen.py emit OUTDIR RTPROBE.t
      reads the probe program's symbol table (where each import lives: code
      or data) and writes
        mod_tables.s  the module's first object: the BRA.W over its header,
                      its export table, a slot and a stub for each C library
                      call it makes, a copy for each C library variable, the
                      heads of libnix's lists, the start of a thread
        app_tables.s  a program's side: a stub for each module call, a
                      stand-in for each piece of module data, its C library
                      table, and struct WXTKApp (__wxtk_app)
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
FORMAT = 2
# the module's own business, never given to a program
PRIVATE = re.compile(r"^(___CTOR_LIST__|___DTOR_LIST__|___INIT_LIST__|___EXIT_LIST__|___LIB_LIST__|"
                     r"__EH_FRAME_BEGINS__|__EH_FRAME_OBJECTS__|__EH_FRAME_BEGIN__|__EH_FRAME_OBJECT__|"
                     r"___a4_init|___data_size|___datadata_relocs|___dso_handle|"
                     r"__wxtk_.*|___wxtk_.*|_wxtk_.*|___wrap_.*|___real_.*)$")
TYPEDATA = re.compile(r"^__ZT[VISTC]")      # vtables, typeinfo and their names: constants
ROSECS = (".data.rel.ro", ".rodata", ".gcc_except_table")
# the C library calls the module keeps for itself (wxtk_module_rt.c)
WRAPPED = ("pthread_create", "pthread_once", "pthread_key_create")


def fnv(name):
    h = 0x811C9DC5
    for b in name.encode():
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def parse_syms(text):
    """[(name, value, flags, section)] from objdump -t"""
    syms = []
    for line in text.splitlines():
        t = line.split()
        if len(t) >= 5 and re.match(r"^[0-9a-f]{8}$", t[0]):
            syms.append((t[-1], int(t[0], 16), "".join(t[1:-4]), t[-4]))
    return syms


def objdump(path):
    """(sections {name: size}, symbols)"""
    out = subprocess.run([OBJDUMP, "-h", "-t", path], capture_output=True, text=True, check=True).stdout
    secs = {}
    head, _, table = out.partition("SYMBOL TABLE:")
    for line in head.splitlines():
        t = line.split()
        if len(t) >= 7 and t[0].isdigit():
            secs[t[1]] = int(t[2], 16)
    return secs, parse_syms(table)


def undefined_in(log):
    """the symbols a link's complaints say nothing defines"""
    out = set()
    for line in open(log, errors="replace"):
        m = re.search(r"undefined reference to [`'\"]([^'`\"]+)['`\"]", line)
        if m:
            out.add(m.group(1))
    return out


def as_linked(names, wanted):
    """ld names a symbol with or without its leading underscore"""
    return {n for n in wanted if n in names or n[1:] in names}


# ---- scan ---------------------------------------------------------------------------

def member_info(m, arch):
    secs, syms = objdump(m)
    defs, undef, linkonce = {}, set(), set()
    for name, val, flags, sec in syms:
        if sec == "*UND*":
            undef.add(name)
            continue
        if sec == "*ABS*" or not ("g" in flags or "w" in flags) or PRIVATE.match(name):
            continue
        ro = sec.startswith(ROSECS) or bool(TYPEDATA.match(name))
        code = sec.startswith(".text") and not TYPEDATA.match(name)
        if sec not in ("*COM*", ".text", ".data", ".bss", ".rodata"):
            linkonce.add(name)                  # an inline function's or template's own section
        defs[name] = {"code": code, "ro": ro, "weak": "w" in flags and "g" not in flags}
    return {"path": m, "arch": arch, "defs": defs, "undef": sorted(undef), "linkonce": sorted(linkonce)}


def scan(outdir, archives):
    xdir = os.path.join(outdir, "x")
    members = []
    for i, a in enumerate(archives):
        d = os.path.join(xdir, str(i))
        os.makedirs(d, exist_ok=True)
        names = subprocess.run([AR, "t", a], capture_output=True, text=True, check=True).stdout.split()
        subprocess.run([AR, "x", os.path.abspath(a)], cwd=d, check=True)
        if len(set(names)) != len(names):
            sys.exit("wxtk-gen: %s has members of the same name" % a)
        members += [(os.path.join(d, n), i) for n in names]
    info = [member_info(m, i) for m, i in members]
    # a symbol two members of one archive both define strongly is an inline
    # function or template (a copy in each object; the linker keeps one): it
    # never decides which member a link takes
    seen, shared = set(), set()
    for mi in info:
        for n, d in mi["defs"].items():
            if not d["weak"]:
                if (mi["arch"], n) in seen:
                    shared.add(n)
                seen.add((mi["arch"], n))
    taken, cur, cur_arch = set(), set(), None
    chosen, skipped = [], []
    for mi in info:
        if mi["arch"] != cur_arch:
            taken |= cur
            cur, cur_arch = set(), mi["arch"]
        own = {n for n, d in mi["defs"].items() if not d["weak"]} - shared - set(mi["linkonce"])
        if own & taken:
            skipped.append(os.path.basename(mi["path"]))
            continue
        cur |= own
        for n in mi["defs"]:
            if n in shared or n in mi["linkonce"]:
                mi["defs"][n]["weak"] = True
        chosen.append(mi)
    with open(os.path.join(outdir, "members.rsp"), "w") as f:
        f.write("\n".join(mi["path"] for mi in chosen) + "\n")
    with open(os.path.join(outdir, "members.json"), "w") as f:
        json.dump(chosen, f)
    print("wxtk-gen: %d members (%d left out: %s)" % (len(chosen), len(skipped), " ".join(skipped)))


# ---- link: what the module gives and takes -----------------------------------------------

CXXFILT = os.environ.get("CXXFILT", "m68k-amigaos-c++filt")


def functions_among(names):
    """which C++ names are functions: in a hunk file a constant table sits in
    the code like a function, so the demangled name tells (a function's has
    its parameters); a C name in the code is taken as a function"""
    cxx = [n for n in names if n.startswith("__Z")]
    out = subprocess.run([CXXFILT], input="\n".join(n[1:] for n in cxx) + "\n",
                         capture_output=True, text=True, check=True).stdout.splitlines()
    return {n for n, d in zip(cxx, out) if "(" in d}


def full_names(path):
    """the names nm gives, and a lookup from a cut-short name to its whole one"""
    names = set()
    for line in open(path, errors="replace"):
        t = line.split()
        if len(t) == 3 and t[1] not in "UuwvV":
            names.add(t[2])
    return names


def link(outdir, mod_t, mod_log, names_txt):
    whole = full_names(names_txt)
    bypre = {}
    for n in whole:
        if len(n) > 64:
            bypre.setdefault(n[:64], []).append(n)
    cut = 0

    def wholename(n):
        nonlocal cut
        if n in whole or len(n) <= 64:
            return n
        c = [x for x in bypre.get(n[:64], ()) if x.startswith(n)]
        if len(c) == 1:
            cut += 1
            return c[0]
        return None

    chosen = json.load(open(os.path.join(outdir, "members.json")))
    wx = {}
    for mi in chosen:
        for n, d in mi["defs"].items():
            if n not in wx or wx[n]["weak"]:
                wx[n] = d
    syms = parse_syms(open(mod_t).read())
    bysec = {}
    for name, val, flags, sec in syms:
        bysec.setdefault(sec, []).append(val)
    for v in bysec.values():
        v.sort()
    ends = {sec: max(v) + 4 for sec, v in bysec.items()}
    exports = {}
    textnames = [wholename(n) for n, v, f, sec in syms if sec == ".text" and wholename(n)]
    funcs = functions_among(textnames)
    for name, val, flags, sec in syms:
        if sec in ("*UND*", "*ABS*") or "g" not in flags and "w" not in flags or PRIVATE.match(name):
            continue
        name = wholename(name)
        if name is None:
            continue                            # cut short past telling: a program can't name it either way
        d = wx.get(name)
        if d:       # the member's own section says (.text*, or .rodata/.data.rel.ro for a table)
            code = d["code"] and not d["ro"]
        else:       # the C++ runtime's: the demangled name says
            code = sec == ".text" and not TYPEDATA.match(name) and (name in funcs or not name.startswith("__Z"))
        ro = (d["ro"] if d else False) or bool(TYPEDATA.match(name)) or sec == ".text"
        later = [x for x in bysec[sec] if x > val]
        size = (min(later) if later else ends[sec]) - val
        exports[name] = {"kind": "code" if code else "data",
                         "own": not code and not ro and sec in (".data", ".bss"),
                         "weak": d["weak"] if d else ("w" in flags and "g" not in flags),
                         "size": max(min(size, 1 << 20), 2)}
    missing = undefined_in(mod_log)
    # (ld names them as linked, with C's leading underscore)
    # what --wrap leaves the module's own calls taking: the C library's own
    missing = {("_" + n[len("___real_"):]) if n.startswith("___real_") else n for n in missing}
    imports = sorted(n for n in missing if not PRIVATE.match(n))
    for n in imports:
        if n in exports:
            sys.exit("wxtk-gen: %s is both given and taken" % n)
    with open(os.path.join(outdir, "state.json"), "w") as f:
        json.dump({"exports": exports, "imports": imports, "absent": []}, f)
    write_probe(outdir, imports)
    ncode = sum(1 for d in exports.values() if d["kind"] == "code")
    nown = sum(1 for d in exports.values() if d["own"])
    print("wxtk-gen: the module gives %d (%d calls, %d data, %d of them each program's own; %d names made whole) and takes %d"
          % (len(exports), ncode, len(exports) - ncode, nown, cut, len(imports)))


def write_probe(outdir, imports):
    with open(os.path.join(outdir, "rtprobe.s"), "w") as f:
        f.write("\t.text\n\t.globl _wxtk_rtprobe\n_wxtk_rtprobe:\n")
        for n in imports:
            f.write("\t.long %s\n" % n)


def absent(outdir, probe_log):
    st = json.load(open(os.path.join(outdir, "state.json")))
    missing = undefined_in(probe_log)
    gone = [n for n in st["imports"] if n in missing or n[1:] in missing]
    st["imports"] = [n for n in st["imports"] if n not in gone]
    st["absent"] = gone
    with open(os.path.join(outdir, "state.json"), "w") as f:
        json.dump(st, f)
    write_probe(outdir, st["imports"])
    if gone:
        print("wxtk-gen: no C library has these, so they trap in the module: %s" % " ".join(gone))


# ---- emit -----------------------------------------------------------------------------

def emit(outdir, rtprobe_t):
    st = json.load(open(os.path.join(outdir, "state.json")))
    defs, imports, gone = st["exports"], st["imports"], st["absent"]
    where, bysec = {}, {}
    for name, val, flags, sec in parse_syms(open(rtprobe_t).read()):
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
        if sec == ".text" and not TYPEDATA.match(n):
            impc.append(n)
        else:
            later = [x for x in bysec[sec] if x > val]
            size = (min(later) - val) if later else 4
            impd.append((n, max(min(size, 4096), 4)))
    impc.sort(key=fnv)
    impd.sort(key=lambda x: fnv(x[0]))

    exports = sorted(defs, key=fnv)
    hashes = [fnv(n) for n in exports]
    for a, b, n in zip(hashes, hashes[1:], exports[1:]):
        if a == b:
            sys.exit("wxtk-gen: two exports share a hash: %s" % n)
    if len(set(fnv(n) for n in imports)) != len(imports):
        sys.exit("wxtk-gen: two imports share a hash")
    buildid = fnv("\n".join(exports) + "\n--\n" + "\n".join(imports))

    # ---- the module's first object (built -fbaserel32) -----------------------
    m = []
    w = m.append
    w("| wx.module's tables (wxtk-gen.py)\n\t.text\n\t.globl _wxtk_module_start\n_wxtk_module_start:\n")
    w("\tbra.w Lself\n")
    w("\t.long 0x%08x,%d,0x%08x\n" % (MAGIC, FORMAT, buildid))
    w("\t.long _wxtk_module_entry,_wxtk_module_exit\n")
    w("\t.long ___a4_init,___data_size,___datadata_relocs\n")
    w("\t.long %d,Lexph,Lexpa,Lexpo\n" % len(exports))
    w("\t.long %d,Limpch,Lslots\n" % len(impc))
    w("\t.long %d,Limpdh,Limpdp,Limpds\n" % len(impd))
    w("\t.long Lnames\n")
    # run as a program, the module does nothing
    w("Lself:\tmoveq #-1,d0\n\trts\n")
    # a new thread (wxtk_module_rt.c's pthread_create): A4 from its start
    # record, the caller's back after
    w("\t.globl _wxtk_thread_start\n_wxtk_thread_start:\n")
    w("\tmove.l a4,-(sp)\n\tmove.l 8(sp),a0\n\tmove.l 8(a0),a4\n\tmove.l a0,-(sp)\n"
      "\tjsr _wxtk_thread_run\n\taddq.l #4,sp\n\tmove.l (sp)+,a4\n\trts\n")
    # a C library call: through its slot in this program's copy
    for i, n in enumerate(impc):
        w("\t.globl %s\n%s:\tmove.l __wxtk_s%d:L(a4),a0\n\tjmp (a0)\n" % (n, n, i))
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
    w("Limpdp:\n")
    for n, _ in impd:
        w("\t.long %s\n" % n)
    w("Limpds:\n")
    for _, s in impd:
        w("\t.long %d\n" % s)
    w("Lexpo:\n")
    for i in range(0, len(exports), 16):
        w("\t.byte %s\n" % ",".join("1" if defs[n]["own"] else "0" for n in exports[i:i + 16]))
    w("Lnames:\n")
    for n in impc + [x for x, _ in impd]:
        w("\t.asciz \"%s\"\n" % n)
    w("\t.data\n\t.align 2\nLslots:\n")
    for i in range(len(impc)):
        w("__wxtk_s%d:\t.long 0\n" % i)
    # what no C library has: an ILLEGAL instruction, in data so the same
    # stand-in serves a call and a read
    for n in gone:
        w("\t.globl %s\n%s:\t.short 0x4afc\n\t.skip 62\n" % (n, n))
    w("\t.bss\n\t.align 2\n")
    for n, s in impd:
        w("\t.globl %s\n%s:\t.skip %d\n" % (n, n, (s + 3) & ~3))
    # libnix's lists: a zero head each, the members' entries, then the end
    # markers (as gfopenrtg's module_start.S)
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
    # a module call: A4 on this program's copy, then a jump the library
    # points at the module. Weak, so the program's own copy of an inline
    # function or template is the one it uses.
    w("Lstubs:\n")
    for n in code:
        w("\t.weak %s\n%s:\n\t.short 0x2879\n\t.long ___wxtk_a4\n\t.short 0x4ef9\n\t.long 0\n" % (n, n))
    w("\t.align 2\nLcodeh:\n")
    for n in code:
        w("\t.long 0x%08x\n" % fnv(n))
    w("Lphoff:\n")
    off = 0
    places = []
    for n in data:
        places.append(n)
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
    # module data: stand-ins. A piece of data the module has weak (a
    # template's static member, an inline function's static, a vtable or
    # typeinfo emitted where used) is strong here, so the program's copies
    # give way to the module's one; the rest is weak, as the stubs are.
    w("\t.bss\n\t.align 2\nLph:\n")
    for n in places:
        w("\t%s %s\n%s:\t.skip %d\n" % (".globl" if defs[n]["weak"] else ".weak", n, n,
                                          (defs[n]["size"] + 4 + 3) & ~3))
    w("Lphe:\t.skip 4\n")
    w("\t.data\n\t.align 2\n\t.globl ___wxtk_a4\n___wxtk_a4:\t.long 0\n")
    w("\t.globl ___wxtk_app\n___wxtk_app:\n")
    w("\t.long 0x%08x,%d,0x%08x\n" % (MAGIC, FORMAT, buildid))
    w("\t.long %d,Lstubs,Lcodeh\n" % len(code))
    w("\t.long %d,Lph,Lphe,Lphoff,Lphh\n" % len(data))
    w("\t.long %d,Lrth,Lrta\n" % len(rt))
    w("\t.long 0,0,0\n")                      # seglist, dirlock, name
    w("\t.long 0,0,0,0\n")                    # a4, copy, header, fixed
    w("\t.skip 200\n")                        # error
    open(os.path.join(outdir, "app_tables.s"), "w").write("".join(a))

    with open(os.path.join(outdir, "exports.txt"), "w") as f:
        for n in exports:
            d = defs[n]
            f.write("%08x %s %s%s%s %d\n" % (fnv(n), n, d["kind"], " own" if d["own"] else "",
                                            " weak" if d["weak"] else "", d["size"]))
    with open(os.path.join(outdir, "imports.txt"), "w") as f:
        for n in impc:
            f.write("%08x %s call\n" % (fnv(n), n))
        for n, s in impd:
            f.write("%08x %s copy %d\n" % (fnv(n), n, s))
        for n in gone:
            f.write("%08x %s traps\n" % (fnv(n), n))
    print("wxtk-gen: build id %08x; a program: %d stubs (%d bytes), %d stand-ins (%d bytes of BSS), %d C library symbols; "
          "the module takes %d calls and %d variables, %d trap" % (buildid, len(code), 12 * len(code), len(data), off,
                                                                 len(rt), len(impc), len(impd), len(gone)))


if __name__ == "__main__":
    a = sys.argv
    if len(a) >= 3 and a[1] == "scan":
        scan(a[2], a[3:])
    elif len(a) == 6 and a[1] == "link":
        link(a[2], a[3], a[4], a[5])
    elif len(a) == 4 and a[1] == "absent":
        absent(a[2], a[3])
    elif len(a) == 4 and a[1] == "emit":
        emit(a[2], a[3])
    else:
        sys.exit(__doc__)
