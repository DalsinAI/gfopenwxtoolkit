#!/usr/bin/env python3
# Copyright (c) 2026 AmiCradle Team
"""gfopenwx's setup.h: the base build's (configure's) with the GUI switches
in config/gui-features.txt set to 1 (and those written !NAME set to 0).
    make-setup.py BASE_SETUP_H FEATURES OUT_SETUP_H"""
import re, sys
base, feats, out = sys.argv[1:4]
on, off = set(), set()
for line in open(feats):
    line = line.split('#', 1)[0]
    for w in line.split():
        (off.add(w[1:]) if w.startswith('!') else on.add(w))
s = open(base).read()
seen = set()
def sub(m):
    name = m.group(2)
    if name in on:
        seen.add(name)
        return '%s#define wxUSE_%s 1' % (m.group(1), name)
    return m.group(0)
s = re.sub(r'^(\s*)#\s*define\s+wxUSE_([A-Z0-9_]+)\s+0\b', sub, s, flags=re.M)
def sub_off(m):
    name = m.group(2)
    if name in off:
        seen.add(name)
        return '%s#define wxUSE_%s 0' % (m.group(1), name)
    return m.group(0)
s = re.sub(r'^(\s*)#\s*define\s+wxUSE_([A-Z0-9_]+)\s+1\b', sub_off, s, flags=re.M)
missing = sorted((on | off) - seen)
if missing:
    sys.exit('make-setup: no "#define wxUSE_X 0" for ' + ' '.join(missing))
s = s.replace('#define _WX_SETUP_H_', '#define _WX_SETUP_H_\n/* gfopenwx: configure\'s base setup with the GUI switches of config/gui-features.txt */', 1)
open(out, 'w').write(s)
