#!/bin/sh
# Development build of openwxtoolkit.library (temporary): the whole toolkit
# in a tree of its own, so the wx ports' builds aren't disturbed. Stops
# itself before Cradle's 900 s limit so its log shows how far it got; run
# it again to go on.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK="$HOME/.cache/gfopenwxtoolkit/library-build"
count() { echo "progress $(date +%T): gui objects $(ls "$WORK/gui/obj" 2>/dev/null | grep -c '\.o$'), load $(cut -d' ' -f1-3 /proc/loadavg)"; }
count
ls -la "$WORK" "$WORK/wxgui" 2>&1 | head -20
( while sleep 120; do count; done ) &
P=$!
WORK="$WORK" OUT="${OUT:-out}" timeout 820 sh "$HERE/build-amiga.sh"
rc=$?
kill $P 2>/dev/null
count
echo "build-amiga.sh exit $rc"
exit $rc
