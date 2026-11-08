#!/bin/sh
# libcurl for wxWebRequest: DalsinAI/gfopencurl's build (HTTP and HTTPS,
# TLS through gfopentls.library at run time, HTTP/2 by nghttp2), soft float
# as the rest of the toolkit. Its zlib calls are compiled against wx's
# zlib.h, so they land on libwxzlib's glue to z.library, as wx's own do.
#   TOOLCHAIN=<stove>/prefix WORK=<build tree> SRC=<wx tree> scripts/build-curl.sh
#   NDK         extra include folder for the NDK (empty: the compiler's own)
#   WXTK_BASEREL  1: -fbaserel32, for the wx module (build-library.sh)
# Result: $WORK/curl/include (curl/, nghttp2/) and $WORK/curl/lib
# (libcurl.a, libnghttp2.a). The upstream tarballs are fetched at the
# versions deps.lock gives, into $WORK/dl.
# Copyright (c) 2026 AmiCradle Team
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$HERE/deps.lock"
TOOLCHAIN=${TOOLCHAIN:?the stove prefix, holding bin/m68k-amigaos-gcc}
WORK=${WORK:?the build tree}
SRC=${SRC:?the wxWidgets tree (its src/zlib headers)}
NDK=${NDK:-}
JOBS=${JOBS:-2}
DL="$WORK/dl"; OUT="$WORK/curl"; CS="$WORK/curl-src"
mkdir -p "$DL" "$CS"
BASEREL=""; [ "${WXTK_BASEREL:-0}" = 1 ] && BASEREL=" -fbaserel32"
CPU="-m68040 -noixemul$BASEREL${NDK:+ -idirafter $NDK}"
# gfopencurl's script takes the compiler root (holding prefix/) and CPU flags
OS32_GCC16=$(dirname "$TOOLCHAIN"); OS32_CPU_FLAGS="$CPU"
export OS32_GCC16 OS32_CPU_FLAGS JOBS
[ "$OS32_GCC16/prefix" = "$TOOLCHAIN" ] || { echo "build-curl: TOOLCHAIN must be <root>/prefix" >&2; exit 1; }

fetch() { # url sha256 -> $DL/<name>
    f="$DL/$(basename "$1")"
    if [ ! -f "$f" ] || [ "$(sha256sum "$f" | cut -d' ' -f1)" != "$2" ]; then
        curl -fsSL --retry 3 -o "$f.part" "$1"; mv "$f.part" "$f"
    fi
    [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$2" ] || { echo "build-curl: $f: SHA-256 mismatch" >&2; exit 1; }
}
repo() { # name url commit -> $CS/name at that commit
    d="$CS/$1"
    if [ "$(git -C "$d" rev-parse HEAD 2>/dev/null || true)" != "$3" ]; then
        rm -rf "${d:?}"; git init -q "$d"
        git -C "$d" fetch -q --depth 1 "$2" "$3"
        git -C "$d" checkout -q FETCH_HEAD
    fi
}

# made again when the pins or the flags change
MARK=$(echo "$GFOPENCURL_COMMIT $GFOPENTLS_COMMIT $CURL_SHA256 $NGHTTP2_SHA256 $CPU wxzlib-1" | sha256sum | cut -c1-16)
if [ "$(cat "$OUT/.built" 2>/dev/null || true)" = "$MARK" ] && [ -f "$OUT/lib/libcurl.a" ]; then
    echo "build-curl: up to date"; exit 0
fi
rm -rf "$OUT"; mkdir -p "$OUT/include" "$OUT/lib"
repo gfopentls "$GFOPENTLS_REPO" "$GFOPENTLS_COMMIT"
repo gfopencurl "$GFOPENCURL_REPO" "$GFOPENCURL_COMMIT"; G="$CS/gfopencurl"
rm -rf "$G/work" "$G/tarballs"; mkdir -p "$G/tarballs"
fetch "$CURL_URL" "$CURL_SHA256"; cp "$DL/$(basename "$CURL_URL")" "$G/tarballs/"
fetch "$NGHTTP2_URL" "$NGHTTP2_SHA256"; cp "$DL/$(basename "$NGHTTP2_URL")" "$G/tarballs/"

# zlib as curl sees it: wx's zlib.h (its calls renamed wx_zlib_*, which
# libwxzlib's glue answers in the program's link); the library CMake asks
# for is an empty one, since curl's checks only compile
Z="$WORK/curl-zlib"; rm -rf "$Z"; mkdir -p "$Z/include" "$Z/lib"
cp "$SRC/src/zlib/zlib.h" "$SRC/src/zlib/zconf.h" "$Z/include/"
"$TOOLCHAIN/bin/m68k-amigaos-ar" rcs "$Z/lib/libz.a"

# (gfopencurl's own smoke test, linked last, wants hard-float maths and a
# zlib of standard names: neither is this build's, and the library is made
# before it)
PREFIX="$OUT" TARBALLS="$G/tarballs" DEPS_PREFIX="$Z" OPENTLS_INCLUDE="$CS/gfopentls/include" \
    sh "$G/build.sh" > "$WORK/curl-build.log" 2>&1 \
    || [ -f "$G/work/curl/build/lib/libcurl.a" ] || { tail -40 "$WORK/curl-build.log" >&2; exit 1; }
[ -f "$OUT/lib/libcurl.a" ] || cp "$G/work/curl/build/lib/libcurl.a" "$OUT/lib/"
[ -f "$OUT/include/curl/curl.h" ] || { mkdir -p "$OUT/include/curl"; cp "$G/work/curl/curl-8.22.0/include/curl/"*.h "$OUT/include/curl/"; }
[ -f "$OUT/lib/libnghttp2.a" ] || { echo "build-curl: no libnghttp2.a" >&2; exit 1; }
# curl's zlib calls must be wx's renamed ones (else the program needs a second zlib)
if "$TOOLCHAIN/bin/m68k-amigaos-nm" -u "$OUT/lib/libcurl.a" 2>/dev/null | grep -qE '^ *U _?(inflate|inflateEnd|zlibVersion)$'; then
    echo "build-curl: libcurl.a calls zlib by its plain names" >&2; exit 1
fi
rm -rf "$G/work"
echo "$MARK" > "$OUT/.built"
ls -l "$OUT/lib"
