# (temporary) dump.txt's "M0+off" / "P0+off" against the module's and the
# program's link maps: the function each address is in.
import re, subprocess, sys, bisect

def syms(path):
    out = []
    try:
        for line in open(path, errors="replace"):
            m = re.match(r'^\s+0x([0-9a-f]{8})\s+(\S+)\s*$', line)
            if m and not m.group(2).startswith('0x'):
                out.append((int(m.group(1), 16), m.group(2)))
    except OSError as e:
        print(e)
    out.sort()
    return out

tables = {"M": syms(sys.argv[2]), "P": syms(sys.argv[3])}
names = []
for line in open(sys.argv[1]):
    m = re.search(r'([MP])0\+([0-9a-f]+)', line)
    if not m:
        print(line.rstrip()); continue
    t = tables[m.group(1)]; off = int(m.group(2), 16)
    i = bisect.bisect_right([a for a, _ in t], off) - 1
    name = "%s+%x" % (t[i][1], off - t[i][0]) if i >= 0 else "?"
    names.append(name)
    print(line.rstrip(), name)
dem = subprocess.run(["m68k-amigaos-c++filt", "-n"], input="\n".join(names), capture_output=True, text=True).stdout
print("== demangled"); print(dem)
