// wx's zlib streams on z.library: a buffer through wxZlibOutputStream and
// back through wxZlibInputStream (zlib and gzip), the checksums, and
// zlib's gz* file calls (T:ztest.gz, two members, and a plain file read
// as it is), each line "ok" or "FAIL".
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

// gzopen/gzwrite/gzputc/gzflush, a second member appended, then gzread
// back, gzgets after gzrewind, gztell, and a plain file through gzread
static bool GzFiles()
{
    static char big[60000], back[70000];
    for ( size_t i = 0; i < sizeof big; i++ )
        big[i] = "life rle\n"[i % 9] ^ (char)(i / 7000);
    gzFile w = gzopen("T:ztest.gz", "wb9");
    if ( !w )
        return false;
    for ( int i = 0; i < 5; i++ )
        gzputc(w, 'x');
    gzwrite(w, big, sizeof big);
    gzflush(w, Z_SYNC_FLUSH);
    gzputs(w, "end\n");
    bool ok = gztell(w) == (z_off_t)(5 + sizeof big + 4) && gzclose(w) == Z_OK;
    w = gzopen("T:ztest.gz", "ab");
    ok = ok && w && gzputs(w, "two\n") == 4 && gzclose(w) == Z_OK;
    gzFile r = gzopen("T:ztest.gz", "rb");
    if ( !ok || !r )
        return false;
    int n = 0, k;
    while ( (k = gzread(r, back + n, 7777)) > 0 )
        n += k;
    ok = n == (int)(5 + sizeof big + 8) && memcmp(back + 5, big, sizeof big) == 0
         && memcmp(back + n - 8, "end\ntwo\n", 8) == 0 && gzeof(r);
    char line[16];
    ok = ok && gzrewind(r) == 0 && gzgets(r, line, sizeof line) && strncmp(line, "xxxxxlife", 9) == 0;
    gzclose(r);
    FILE *f = fopen("T:ztest.txt", "w");
    if ( !f )
        return false;
    fputs("plain\n", f);
    fclose(f);
    r = gzopen("T:ztest.txt", "r");
    n = r ? gzread(r, back, 100) : -1;
    if ( r )
        gzclose(r);
    remove("T:ztest.gz");
    remove("T:ztest.txt");
    return ok && n == 6 && memcmp(back, "plain\n", 6) == 0;
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
    check(GzFiles(), "gz file calls");
    printf("ztest: %d failed\n", fails);
    return fails != 0;
}
