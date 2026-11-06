// wx's zlib streams on z.library: a buffer through wxZlibOutputStream and
// back through wxZlibInputStream (zlib and gzip), and wxZipOutputStream's
// CRC, each line "ok" or "FAIL".
// Copyright (c) 2026 AmiCradle Team
#include <wx/init.h>
#include <wx/mstream.h>
#include <wx/zstream.h>
#include <wx/crt.h>
#include <zlib.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(bool ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if ( !ok )
        fails++;
}

static bool RoundTrip(int flags)
{
    char in[20000];
    for ( size_t i = 0; i < sizeof in; i++ )
        in[i] = "AmiCradle wx on z.library "[i % 26] + (i / 2600);
    wxMemoryOutputStream packed;
    {
        wxZlibOutputStream z(packed, 6, flags);
        z.Write(in, sizeof in);
    }
    wxMemoryInputStream src(packed);
    wxZlibInputStream unz(src, flags);
    char out[sizeof in];
    unz.Read(out, sizeof out);
    printf("     %u bytes packed to %u\n", (unsigned)sizeof in, (unsigned)packed.GetSize());
    return unz.LastRead() == sizeof out && memcmp(in, out, sizeof in) == 0 && packed.GetSize() < sizeof in;
}

int main(int argc, char **argv)
{
    wxInitializer init(argc, argv);
    printf("     zlibVersion %s\n", zlibVersion());
    check(wxZlibInputStream::CanHandleGZip(), "gzip handled");
    check(RoundTrip(wxZLIB_ZLIB), "zlib round trip");
    check(RoundTrip(wxZLIB_GZIP), "gzip round trip");
    check(crc32(0, (const Bytef *)"123456789", 9) == 0xCBF43926UL, "crc32 check value");
    check(adler32(1, (const Bytef *)"Wikipedia", 9) == 0x11E60398UL, "adler32 check value");
    printf("ztest: %d failed\n", fails);
    return fails != 0;
}
