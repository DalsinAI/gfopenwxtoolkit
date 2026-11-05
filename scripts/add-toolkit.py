#!/usr/bin/env python3
# Copyright (c) 2026 AmiCradle Team
"""Teaches an unpacked wxWidgets tree the "openlayout" toolkit
(__WXOPENLAYOUT__), the way wxWidgets' own ports are wired in.

wx picks each class's port in a dispatch block:
    #elif defined(__WXQT__)
        ...
        #include "wx/qt/button.h"
For every such block that includes a wx/qt header this repository also has
(include/wx/openlayout/<same name>), a copy of it for openlayout is put in
front of it, naming our header and our classes (wxWindowQt becomes
wxWindowOpenLayout). Where we have no such
header, wx's generic code is used, as on a port without a native one.

Then config/toolkit-edits.txt: lines a port must also change by hand,
"FILE<TAB>OLD<TAB>NEW", each OLD found exactly once.

    add-toolkit.py WXTREE PORTDIR
Idempotent: a tree it has done carries .openlayout-toolkit.
"""
import os, re, sys

tree, port = sys.argv[1], sys.argv[2]
mark = os.path.join(tree, ".openlayout-toolkit")
if os.path.exists(mark):
    sys.exit(0)

ours = os.path.join(port, "include/wx/openlayout")
have = set()
for root, _, files in os.walk(ours):
    for f in files:
        have.add(os.path.relpath(os.path.join(root, f), ours))

# definition-only Qt blocks this port doesn't copy (it has its own say in
# config/toolkit-edits.txt, or wx's generic code is what it wants)
NOT_LIKE_QT = {"wx/spinctrl.h", "wx/calctrl.h", "wx/rawbmp.h", "wx/print.h", "wx/joystick.h", "wx/fontutil.h"}

qt_elif = re.compile(r'^\s*#\s*elif\s+defined\s*\(\s*__WXQT__\s*\)\s*$')
directive = re.compile(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b')
qt_inc = re.compile(r'#\s*include\s+[<"]wx/qt/([^">]+)[">]')

branches = 0
for root, _, files in os.walk(os.path.join(tree, "include/wx")):
    rel = os.path.relpath(root, os.path.join(tree, "include/wx"))
    if rel.split(os.sep)[0] in ("qt", "openlayout", "msw", "osx", "gtk", "gtk1", "motif", "x11", "dfb", "univ", "android"):
        continue
    for f in files:
        if not f.endswith(".h"):
            continue
        p = os.path.join(root, f)
        lines = open(p, encoding="utf-8", errors="surrogateescape").read().split("\n")
        out = []
        i = 0
        hit = False
        while i < len(lines):
            line = lines[i]
            if qt_elif.match(line):
                # the block runs to the next directive at the same depth
                depth, j, name = 0, i + 1, None
                while j < len(lines):
                    m = directive.match(lines[j])
                    if m:
                        k = m.group(1)
                        if k in ("if", "ifdef", "ifndef"):
                            depth += 1
                        elif k == "endif":
                            if depth == 0:
                                break
                            depth -= 1
                        elif depth == 0:
                            break
                    mi = qt_inc.search(lines[j])
                    if mi and name is None:
                        name = mi.group(1)
                    j += 1
                indent = re.match(r'^(\s*)', line).group(1)
                if name in have:
                    # Qt's block for us: its definitions too (a default
                    # bitmap type, wxWindowQt as wxWindow), names changed
                    out.append(indent + "#elif defined(__WXOPENLAYOUT__)")
                    for b in lines[i + 1:j]:
                        b = b.replace("wx/qt/", "wx/openlayout/")
                        b = re.sub(r'\b(wx\w+)Qt\b', r'\1OpenLayout', b)
                        out.append(b)
                    branches += 1
                    hit = True
                elif name is None and os.path.relpath(p, os.path.join(tree, "include")) not in NOT_LIKE_QT:
                    # a block of definitions only: do as Qt does
                    out.append(indent + "#elif defined(__WXOPENLAYOUT__)")
                    out.extend(lines[i + 1:j])
                    branches += 1
                    hit = True
            out.append(line)
            i += 1
        if hit:
            open(p, "w", encoding="utf-8", errors="surrogateescape").write("\n".join(out))

edits = 0
ed = os.path.join(port, "config/toolkit-edits.txt")
if os.path.exists(ed):
    for n, line in enumerate(open(ed, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        fn, old, new = line.split("\t")
        old = old.replace("\\n", "\n"); new = new.replace("\\n", "\n")
        p = os.path.join(tree, fn)
        s = open(p, encoding="utf-8", errors="surrogateescape").read()
        if s.count(old) != 1:
            sys.exit("add-toolkit: %s line %d: %r found %d times in %s" % (ed, n, old, s.count(old), fn))
        open(p, "w", encoding="utf-8", errors="surrogateescape").write(s.replace(old, new))
        edits += 1

open(mark, "w").write("%d dispatch branches, %d edits\n" % (branches, edits))
print("add-toolkit: %d dispatch branches, %d edits, %d port headers" % (branches, edits, len(have)))
