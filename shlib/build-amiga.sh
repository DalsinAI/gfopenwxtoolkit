#!/bin/sh
# Development build of openwxtoolkit.library (temporary): the whole toolkit
# in a tree of its own, so the wx ports' builds aren't disturbed.
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
WORK="$HOME/.cache/gfopenwxtoolkit/library-build" OUT="${OUT:-out}" sh "$HERE/build-amiga.sh"
