#!/bin/sh
# Cradle's stove target for openwxtoolkit.library: wx built -fbaserel32
# (WXTK_BASEREL=1, -Os), the library, its wx module and the tests linked
# shared. Stops before Cradle's 900 s limit with exit-cut; run it again to
# go on where it stopped (the build tree lives outside the checkout).
#   OUT/Libs/openwxtoolkit.library, OUT/Libs/openwxtoolkit/wx.module,
#   OUT/shared/<tests>, OUT/build.log, OUT/exit-N
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
C="$HOME/.cache/gfopenwxtoolkit"
W="${WORK:-$C/library-build-os-baserel}"
OUTD="${OUT:-out}"; mkdir -p "$OUTD"
# (the compilers' temporary files go under the cache, not the shared /tmp)
export TMPDIR="$C/tmp"; mkdir -p "$TMPDIR"
WXTK_OPT=Os WXTK_BASEREL=1 JOBS=${JOBS:-6} WORK="$W" OUT="$W/out" STOVE="${STOVE:?}" \
    nice -n 19 timeout 830 sh "$HERE/build-amiga.sh" > "$OUTD/build.log" 2>&1
rc=$?
grep -v -e 'warning' -e '^ *|' -e '^ *[0-9]* |' -e 'In file included' -e '^ *from ' "$OUTD/build.log" | cut -c1-300 | tail -n 30
if [ $rc = 124 ]; then : > "$OUTD/exit-cut"; echo "cut at 830 s: run again to go on"; exit 0; fi
[ $rc = 0 ] || { : > "$OUTD/exit-$rc"; exit 0; }
cp -a "$W/out/Libs" "$OUTD/"
[ -d "$W/out/shared" ] && cp -a "$W/out/shared" "$OUTD/"
ls -lR "$OUTD/Libs" "$OUTD/shared"
: > "$OUTD/exit-0"
# the package (Installer script, library, module, tests)
sh "$HERE/scripts/package.sh" "$OUTD" "$OUTD" "${WXTK_PKG_VERSION:-1.0-rc2}" || echo "package failed"
