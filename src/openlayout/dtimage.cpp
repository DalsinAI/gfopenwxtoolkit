/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/dtimage.cpp
// Purpose:     wxPNGHandler and wxJPEGHandler on datatypes.library: every
//              decoder is a datatype, so wxImage reads PNG and JPEG through
//              the system's picture datatypes (gfopenpng.datatype, the
//              JPEG datatype) and no libpng or libjpeg is compiled in. PNG
//              is written through gfopenpng.datatype's OIM_ENCODEPNG.
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_IMAGE && wxUSE_STREAMS

#ifndef WX_PRECOMP
    #include "wx/image.h"
    #include "wx/log.h"
    #include "wx/intl.h"
#endif
#include "wx/imagpng.h"
#include "wx/imagjpeg.h"
#include "wx/versioninfo.h"
#include "wx/wfstream.h"
#include "wx/filename.h"

#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classusr.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>

// datatypes.library, opened while a picture is read or written
#define __NOLIBBASE__
#include <proto/datatypes.h>
#undef __NOLIBBASE__
static struct Library *DataTypesBase;

#include <string.h>

// gfopenpng.datatype's PNG out (gfopendatatypes Datatypes/include/datatypes/gfopenimage.h)
#define wxOL_OIM_ENCODEPNG (TAG_USER + 0x0DA15901)
struct wxOLEncodePNG
{
    ULONG MethodID;
    struct GadgetInfo *oiep_GInfo;
    const UBYTE *oiep_Pixels;
    ULONG oiep_Width, oiep_Height;
    ULONG oiep_Stride;
    ULONG oiep_Flags;
    LONG oiep_Level;
    UBYTE *oiep_Data;
    ULONG oiep_Size;
    LONG oiep_Error;
};
#define wxOL_OIEF_OPAQUE 2UL

namespace
{

struct DTLib
{
    DTLib() { DataTypesBase = OpenLibrary((CONST_STRPTR)"datatypes.library", 39); }
    ~DTLib()
    {
        if ( DataTypesBase )
            CloseLibrary(DataTypesBase);
        DataTypesBase = NULL;
    }
    bool Ok() const { return DataTypesBase != NULL; }
};

// the whole stream, for the datatype to read from memory
bool ReadAll(wxInputStream& stream, wxMemoryBuffer& buf)
{
    char chunk[8192];
    while ( stream.CanRead() || !stream.Eof() )
    {
        stream.Read(chunk, sizeof chunk);
        const size_t n = stream.LastRead();
        if ( !n )
            break;
        buf.AppendData(chunk, n);
    }
    return buf.GetDataLen() > 0;
}

// a picture object for the data: from memory, else (a datatype that reads
// only files) from a file in T: it is written to
Object *NewPicture(const wxMemoryBuffer& buf, wxString& tmpName)
{
    Object *o = NewDTObject((APTR)NULL, DTA_SourceType, DTST_MEMORY,
                            DTA_SourceAddress, (ULONG)buf.GetData(), DTA_SourceSize, (ULONG)buf.GetDataLen(),
                            DTA_GroupID, GID_PICTURE, PDTA_DestMode, PMODE_V43, PDTA_Remap, FALSE, TAG_DONE);
    if ( o )
        return o;
    tmpName = wxString::Format("T:wxdt.%08lx", (unsigned long)FindTask(NULL));
    BPTR f = Open((CONST_STRPTR)(const char *)tmpName.mb_str(), MODE_NEWFILE);
    if ( !f )
        return NULL;
    const LONG wrote = Write(f, (APTR)buf.GetData(), (LONG)buf.GetDataLen());
    Close(f);
    if ( wrote != (LONG)buf.GetDataLen() )
        return NULL;
    return NewDTObject((APTR)(const char *)tmpName.mb_str(), DTA_SourceType, DTST_FILE,
                       DTA_GroupID, GID_PICTURE, PDTA_DestMode, PMODE_V43, PDTA_Remap, FALSE, TAG_DONE);
}

bool LoadPicture(wxImage *image, wxInputStream& stream, bool verbose, const char *what)
{
    DTLib lib;
    if ( !lib.Ok() )
    {
        if ( verbose )
            wxLogError(_("datatypes.library could not be opened to read a %s image."), what);
        return false;
    }
    wxMemoryBuffer buf;
    if ( !ReadAll(stream, buf) )
        return false;
    wxString tmp;
    Object *o = NewPicture(buf, tmp);
    bool ok = false;
    if ( o )
    {
        struct BitMapHeader *bmh = NULL;
        GetDTAttrs(o, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE);
        const int w = bmh ? bmh->bmh_Width : 0, h = bmh ? bmh->bmh_Height : 0;
        UBYTE *argb = w > 0 && h > 0 ? (UBYTE *)AllocVec((ULONG)w * h * 4, MEMF_ANY) : NULL;
        if ( argb )
        {
            struct pdtBlitPixelArray m;
            m.MethodID = PDTM_READPIXELARRAY;
            m.pbpa_PixelData = argb;
            m.pbpa_PixelFormat = PBPAFMT_ARGB;
            m.pbpa_PixelArrayMod = (ULONG)w * 4;
            m.pbpa_Left = 0;
            m.pbpa_Top = 0;
            m.pbpa_Width = (ULONG)w;
            m.pbpa_Height = (ULONG)h;
            // DoMethodA, not DoDTMethodA (gfopenpng.datatype refuses the latter)
            if ( DoMethodA(o, (Msg)&m) )
            {
                image->Create(w, h, false);
                unsigned char *rgb = image->GetData();
                const UBYTE *p = argb;
                bool alpha = false;
                for ( long i = 0, n = (long)w * h; i < n; i++, p += 4 )
                    if ( p[0] != 0xff )
                        { alpha = true; break; }
                unsigned char *a = NULL;
                if ( alpha )
                {
                    image->SetAlpha();
                    a = image->GetAlpha();
                }
                p = argb;
                for ( long i = 0, n = (long)w * h; i < n; i++, p += 4 )
                {
                    *rgb++ = p[1];
                    *rgb++ = p[2];
                    *rgb++ = p[3];
                    if ( a )
                        *a++ = p[0];
                }
                ok = true;
            }
            FreeVec(argb);
        }
        DisposeDTObject(o);
    }
    if ( !tmp.empty() )
        DeleteFile((CONST_STRPTR)(const char *)tmp.mb_str());
    if ( !ok && verbose )
        wxLogError(_("No datatype could read this %s image."), what);
    return ok;
}

bool HasSignature(wxInputStream& stream, const unsigned char *sig, size_t n)
{
    unsigned char got[8];
    if ( !stream.ReadAll(got, n) )
        return false;
    return memcmp(got, sig, n) == 0;
}

} // anonymous namespace

// ---- PNG -------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxPNGHandler, wxImageHandler);

bool wxPNGHandler::LoadFile(wxImage *image, wxInputStream& stream, bool verbose, int WXUNUSED(index))
{
    return LoadPicture(image, stream, verbose, "PNG");
}

bool wxPNGHandler::SaveFile(wxImage *image, wxOutputStream& stream, bool verbose)
{
    DTLib lib;
    if ( !lib.Ok() || !image->IsOk() )
        return false;
    const int w = image->GetWidth(), h = image->GetHeight();
    UBYTE *argb = (UBYTE *)AllocVec((ULONG)w * h * 4, MEMF_ANY);
    if ( !argb )
        return false;
    const unsigned char *rgb = image->GetData(), *a = image->HasAlpha() ? image->GetAlpha() : NULL;
    const bool mask = image->HasMask();
    const unsigned char mr = image->GetMaskRed(), mg = image->GetMaskGreen(), mb = image->GetMaskBlue();
    UBYTE *p = argb;
    for ( long i = 0, n = (long)w * h; i < n; i++, rgb += 3, p += 4 )
    {
        p[0] = a ? a[i] : (mask && rgb[0] == mr && rgb[1] == mg && rgb[2] == mb) ? 0 : 0xff;
        p[1] = rgb[0];
        p[2] = rgb[1];
        p[3] = rgb[2];
    }
    bool ok = false;
    Object *o = NewDTObject((APTR)NULL, DTA_SourceType, DTST_RAM, DTA_BaseName, (ULONG)"gfopenpng",
                            DTA_GroupID, GID_PICTURE, TAG_DONE);
    if ( o )
    {
        struct wxOLEncodePNG m;
        memset(&m, 0, sizeof m);
        m.MethodID = wxOL_OIM_ENCODEPNG;
        m.oiep_Pixels = argb;
        m.oiep_Width = (ULONG)w;
        m.oiep_Height = (ULONG)h;
        m.oiep_Stride = (ULONG)w * 4;
        m.oiep_Flags = (a || mask) ? 0 : wxOL_OIEF_OPAQUE;
        m.oiep_Level = image->HasOption(wxIMAGE_OPTION_PNG_COMPRESSION_LEVEL)
                       ? image->GetOptionInt(wxIMAGE_OPTION_PNG_COMPRESSION_LEVEL) : -1;
        if ( DoMethodA(o, (Msg)&m) && m.oiep_Data )
        {
            stream.Write(m.oiep_Data, m.oiep_Size);
            ok = stream.LastWrite() == m.oiep_Size;
            FreeVec(m.oiep_Data);
        }
        DisposeDTObject(o);
    }
    FreeVec(argb);
    if ( !ok && verbose )
        wxLogError(_("gfopenpng.datatype could not write the PNG image."));
    return ok;
}

bool wxPNGHandler::DoCanRead(wxInputStream& stream)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    return HasSignature(stream, sig, sizeof sig);
}

/*static*/ wxVersionInfo wxPNGHandler::GetLibraryVersionInfo()
{
    return wxVersionInfo("gfopenpng.datatype", 47, 0, 0, "PNG through datatypes.library");
}

// ---- JPEG ------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxJPEGHandler, wxImageHandler);

bool wxJPEGHandler::LoadFile(wxImage *image, wxInputStream& stream, bool verbose, int WXUNUSED(index))
{
    return LoadPicture(image, stream, verbose, "JPEG");
}

bool wxJPEGHandler::SaveFile(wxImage *WXUNUSED(image), wxOutputStream& WXUNUSED(stream), bool verbose)
{
    // no datatype writes JPEG
    if ( verbose )
        wxLogError(_("JPEG images can't be written: no datatype writes JPEG."));
    return false;
}

bool wxJPEGHandler::DoCanRead(wxInputStream& stream)
{
    static const unsigned char sig[3] = { 0xff, 0xd8, 0xff };
    return HasSignature(stream, sig, sizeof sig);
}

/*static*/ wxVersionInfo wxJPEGHandler::GetLibraryVersionInfo()
{
    return wxVersionInfo("JPEG datatype", 0, 0, 0, "JPEG through datatypes.library");
}

#endif // wxUSE_IMAGE && wxUSE_STREAMS
