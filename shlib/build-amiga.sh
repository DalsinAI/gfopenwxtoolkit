#!/bin/sh
# Development build of openwxtoolkit.library (temporary): the whole toolkit
# in a tree of its own, so the wx ports' builds aren't disturbed. Stops
# itself before Cradle's 900 s limit so its log shows how far it got; run
# it again to go on.
# Copyright (c) 2026 AmiCradle Team
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK="$HOME/.cache/gfopenwxtoolkit/library-build"
OUTD="${OUT:-out}"; mkdir -p "$OUTD"
# Cradle shows a job's output files even when its log is lost: the
# progress goes into their names too
count() {
    n=$(ls "$WORK/gui/obj" 2>/dev/null | grep -c '\.o$'); l=$(cut -d' ' -f1 /proc/loadavg)
    echo "progress $(date +%T): gui objects $n, load $l"
    : > "$OUTD/progress-$(date +%H%M)-gui$n-load$l"
}
count
ls -la "$WORK" "$WORK/wxgui" 2>&1 | head -20
( while sleep 120; do count; done ) &
P=$!
# the whole log goes to a file; the job's log gets its short lines' end
WORK="$WORK" OUT="${OUT:-out}" timeout 820 sh "$HERE/build-amiga.sh" > "$OUTD/build.log" 2>&1
rc=$?
kill $P 2>/dev/null
count
grep -v -e 'warning' -e '^ *|' -e '^ *[0-9]* |' -e 'In file included' -e '^ *from ' "$OUTD/build.log" | cut -c1-400 | tail -n 120
grep 'wxtk-gen' "$OUTD/build.log" | cut -c1-3000
echo "build-amiga.sh exit $rc"
# a failed job hands nothing over: the code goes in a file name and the job ends well, so build.log can be fetched
: > "$OUTD/exit-$rc"
exit 0
