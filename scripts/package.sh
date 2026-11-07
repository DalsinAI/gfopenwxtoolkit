#!/bin/sh
# The GFOpenWX Toolkit package: openwxtoolkit.library, its wx module, the
# shared tests, the documents and the Amiga Installer script, as
# GFOpenWXToolkit-VERSION.lha (and .zip) in DEST, and unpacked in DEST/package.
#   package.sh BUILD-OUT DEST VERSION
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
B=$1; D=$2; V=$3
N="GFOpenWXToolkit-$V"
D=$(CDPATH= cd -- "$D" && pwd)
T="${TMPDIR:-/tmp}/wxtk-pkg-$$"; rm -rf "$T"; mkdir -p "$T/GFOpenWXToolkit/Libs/openwxtoolkit" "$T/GFOpenWXToolkit/Tests" "$T/GFOpenWXToolkit/Docs"
P="$T/GFOpenWXToolkit"
cp "$B/Libs/openwxtoolkit.library" "$P/Libs/"
cp "$B/Libs/openwxtoolkit/wx.module" "$P/Libs/openwxtoolkit/"
for t in basetest wctest sockettest xrcstart minimal; do if [ -f "$B/shared/$t" ]; then cp "$B/shared/$t" "$P/Tests/"; fi; done
cp "$HERE/README.md" "$HERE/DESIGN.md" "$P/Docs/"
cp "$HERE/LICENSE-wxWindows.txt" "$P/Docs/" 2>/dev/null || true
SHA=$(git -C "$HERE" rev-parse --short HEAD 2>/dev/null || echo unknown)
sed -e "s/@VERSION@/$V/" -e "s/@SHA@/$SHA/" "$HERE/package/ReadMe.txt" > "$P/ReadMe.txt"
cp "$HERE/package/Install-GFOpenWXToolkit" "$HERE/package/Install-GFOpenWXToolkit.info" "$T/"
rm -f "$D/$N.lha" "$D/$N.zip"
if command -v lha >/dev/null 2>&1; then (cd "$T" && lha -aq "$D/$N.lha" Install-GFOpenWXToolkit Install-GFOpenWXToolkit.info GFOpenWXToolkit); fi
(cd "$T" && zip -qr "$D/$N.zip" Install-GFOpenWXToolkit Install-GFOpenWXToolkit.info GFOpenWXToolkit)
(cd "$D" && sha256sum "$N".* > "$N.sha256")
# the unpacked package too, for a lab without an LhA (DEST/package/)
rm -rf "$D/package"; cp -a "$T" "$D/package"
rm -rf "$T"
ls -l "$D/$N".*
