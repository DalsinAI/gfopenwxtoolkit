#!/usr/bin/env python3
"""wxtk-absown.py MODULE STATE.JSON

After wx.module is linked: the module data its own code reaches at its
address as loaded (an absolute address in the code, not A4) is one for every
program, so a program's references to it must go there too, not to its copy.
GCC does that for a const object a constructor fills in (wx's event types,
wxEVT_*): another file sees only "extern const" and takes it for read-only.

Reads the module's hunks, finds the code's 32-bit relocations into the data
hunk (leaving out the module's own tables), and clears the "each program's
own" flag of every export they land in. Writes the module back in place.
Copyright (c) 2026 AmiCradle Team
"""
import json
import struct
import sys

HUNK_CODE, HUNK_DATA, HUNK_BSS = 0x3E9, 0x3EA, 0x3EB
HUNK_RELOC32, HUNK_SYMBOL, HUNK_DEBUG, HUNK_END = 0x3EC, 0x3F0, 0x3F1, 0x3F2
HUNK_HEADER, HUNK_DREL32, HUNK_RELOC32SHORT = 0x3F3, 0x3F7, 0x3FC

FNV_OFF, FNV_PRIME = 0x811C9DC5, 0x01000193


def main():
    path, state = sys.argv[1], sys.argv[2]
    buf = bytearray(open(path, "rb").read())
    pos = 0

    def long_():
        nonlocal pos
        v = struct.unpack_from(">L", buf, pos)[0]
        pos += 4
        return v

    def word():
        nonlocal pos
        v = struct.unpack_from(">H", buf, pos)[0]
        pos += 2
        return v

    if long_() != HUNK_HEADER:
        sys.exit("wxtk-absown: not a hunk file")
    while True:
        n = long_()
        if not n:
            break
        pos += n * 4
    nh = long_()
    first, last = long_(), long_()
    pos += (last - first + 1) * 4
    kinds, content = [None] * nh, [None] * nh     # content: (file offset, bytes)
    relocs = [[] for _ in range(nh)]              # (offset in hunk, target hunk)
    h = 0
    while h < nh:
        t = long_() & 0x3FFFFFFF
        if t in (HUNK_CODE, HUNK_DATA):
            n = long_() * 4
            kinds[h] = t
            content[h] = (pos, n)
            pos += n
        elif t == HUNK_BSS:
            long_()
            kinds[h] = t
        elif t == HUNK_RELOC32:
            while True:
                n = long_()
                if not n:
                    break
                target = long_()
                for _ in range(n):
                    relocs[h].append((long_(), target))
        elif t in (HUNK_DREL32, HUNK_RELOC32SHORT):
            words = 0
            while True:
                n = word()
                if not n:
                    break
                target = word()
                words += 2 + n
                for _ in range(n):
                    relocs[h].append((word(), target))
            words += 1
            if words & 1:
                word()
        elif t == HUNK_SYMBOL:
            while True:
                n = long_()
                if not n:
                    break
                pos += (n & 0xFFFFFF) * 4 + 4
        elif t == HUNK_DEBUG:
            pos += long_() * 4
        elif t == HUNK_END:
            h += 1
        else:
            sys.exit("wxtk-absown: hunk type %x" % t)

    code = 0
    if kinds[code] != HUNK_CODE:
        sys.exit("wxtk-absown: the first hunk isn't code")
    datas = [i for i, k in enumerate(kinds) if k == HUNK_DATA]
    if len(datas) != 1:
        sys.exit("wxtk-absown: %d data hunks" % len(datas))
    data = datas[0]
    cbase = content[code][0]

    def at(off):
        return struct.unpack_from(">L", buf, cbase + off)[0]

    # the header (wxtk-gen.py emit): bra.w, then longs
    hdr = [at(2 + 4 * i) for i in range(20)]
    nexp, lexpa, lexpo = hdr[8], hdr[10], hdr[11]
    nimpd, limpdp = hdr[15], hdr[17]
    skip = [(0, 2 + 4 * 20), (lexpa, lexpa + 4 * nexp), (limpdp, limpdp + 4 * nimpd)]
    into = {o: t for o, t in relocs[code]}

    # the exports in the data hunk: where each starts
    starts = sorted((at(lexpa + 4 * i), i) for i in range(nexp) if into.get(lexpa + 4 * i) == data)
    offs = [s for s, _ in starts]

    def export_at(v):
        lo, hi = 0, len(offs)
        while lo < hi:
            mid = (lo + hi) // 2
            if offs[mid] <= v:
                lo = mid + 1
            else:
                hi = mid
        return starts[lo - 1][1] if lo else None

    from collections import Counter
    print("wxtk-absown: hunks %s; relocations by (from, to): %s" % (
        [hex(k) if k else None for k in kinds],
        dict(Counter((hh, t) for hh in range(nh) for _, t in relocs[hh]))))
    print("wxtk-absown: %d exports, table at %x, %d of them in the data hunk (%d)" % (nexp, lexpa, len(starts), data))
    shared = set()
    for o, t in relocs[code]:
        if t != data or any(a <= o < b for a, b in skip):
            continue
        i = export_at(at(o))
        if i is not None and buf[cbase + lexpo + i]:
            shared.add(i)

    names = sorted(json.load(open(state))["exports"], key=fnv)
    for i in sorted(shared):
        buf[cbase + lexpo + i] = 0
    open(path, "wb").write(buf)
    print("wxtk-absown: %d exports the module's code reaches at their loaded address, now one for every program: %s"
          % (len(shared), " ".join(names[i] for i in sorted(shared)[:12]) + (" ..." if len(shared) > 12 else "")))


def fnv(s):
    h = FNV_OFF
    for c in s.encode():
        h = ((h ^ c) * FNV_PRIME) & 0xFFFFFFFF
    return h


if __name__ == "__main__":
    main()
