/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/bitmap.cpp
// Purpose:     wxBitmap: ARGB pixels of the program's own, with a BitMap
//              like the screen's made from them while a wxMemoryDC draws on
//              it or it is blitted
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/image.h"
    #include "wx/palette.h"
    #include "wx/icon.h"
#endif

#include "wx/rawbmp.h"

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/screens.h>
#include <clib/graphics_protos.h>

#include <string.h>
#include <stdlib.h>

#include "private.h"

class wxBitmapRefData : public wxGDIRefData
{
public:
    wxBitmapRefData(int w, int h, int depth)
        : m_width(w), m_height(h), m_depth(depth), m_alpha(false), m_mask(NULL),
          m_bm(NULL), m_bmValid(false), m_bmNewer(false)
    {
        m_pixels = (unsigned char *)malloc((size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1) * 4);
        if ( m_pixels )
        {
            // opaque black
            ULONG *p = (ULONG *)m_pixels;
            for ( size_t i = 0, n = (size_t)w * h; i < n; i++ )
                p[i] = 0xff000000UL;
        }
    }

    wxBitmapRefData(const wxBitmapRefData& d)
        : wxGDIRefData(), m_width(d.m_width), m_height(d.m_height), m_depth(d.m_depth), m_alpha(d.m_alpha),
          m_mask(d.m_mask ? new wxMask(*d.m_mask) : NULL), m_bm(NULL), m_bmValid(false), m_bmNewer(false)
    {
        const_cast<wxBitmapRefData&>(d).Sync();
        size_t n = (size_t)m_width * m_height * 4;
        m_pixels = (unsigned char *)malloc(n ? n : 4);
        if ( m_pixels && d.m_pixels )
            memcpy(m_pixels, d.m_pixels, n);
#if wxUSE_PALETTE
        m_palette = d.m_palette;
#endif
    }

    virtual ~wxBitmapRefData()
    {
        FreeBitMap_();
        delete m_mask;
        free(m_pixels);
    }

    virtual bool IsOk() const wxOVERRIDE { return m_pixels != NULL && m_width > 0 && m_height > 0; }

    void FreeBitMap_()
    {
        if ( m_bm )
        {
            WaitBlit();
            FreeBitMap(m_bm);
        }
        m_bm = NULL;
        m_bmValid = m_bmNewer = false;
    }

    // the BitMap, with the pixels written into it when it isn't up to date
    struct BitMap *BitMap_()
    {
        struct Screen *scr = wxOLGetScreen();
        if ( !scr )
            return NULL;
        if ( !m_bm )
        {
            m_bm = AllocBitMap((ULONG)m_width, (ULONG)m_height, GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH),
                               BMF_MINPLANES, scr->RastPort.BitMap);
            m_bmValid = false;
        }
        if ( m_bm && !m_bmValid )
        {
            Write(m_bm);
            m_bmValid = true;
        }
        return m_bm;
    }

    void Write(struct BitMap *bm)
    {
        struct RastPort rp;
        InitRastPort(&rp);
        rp.BitMap = bm;
        if ( wxOLIsTrueColour(bm) )
        {
            wxOL_CGX_WRITE(m_pixels, 0, 0, m_width * 4, &rp, 0, 0, m_width, m_height);
            return;
        }
        // a palette screen: each pixel in its nearest pen, a row at a time
        for ( int y = 0; y < m_height; y++ )
        {
            const ULONG *row = (const ULONG *)(m_pixels + (size_t)y * m_width * 4);
            int x0 = 0;
            while ( x0 < m_width )
            {
                ULONG c = row[x0] & 0xffffff;
                int x1 = x0 + 1;
                while ( x1 < m_width && (row[x1] & 0xffffff) == c )
                    x1++;
                SetAPen(&rp, (ULONG)wxOLPenFor(c));
                RectFill(&rp, x0, y, x1 - 1, y);
                x0 = x1;
            }
        }
    }

    // what was drawn in the BitMap back into the pixels
    void Sync()
    {
        if ( !m_bm || !m_bmNewer )
            return;
        m_bmNewer = false;
        if ( !wxOLIsTrueColour(m_bm) )
            return;
        struct RastPort rp;
        InitRastPort(&rp);
        rp.BitMap = m_bm;
        WaitBlit();
        if ( m_alpha )
        {
            // drawing leaves the alpha as it was
            const size_t n = (size_t)m_width * m_height;
            unsigned char *a = (unsigned char *)malloc(n);
            if ( a )
                for ( size_t i = 0; i < n; i++ )
                    a[i] = m_pixels[i * 4];
            wxOL_CGX_READ(m_pixels, 0, 0, m_width * 4, &rp, 0, 0, m_width, m_height);
            if ( a )
            {
                for ( size_t i = 0; i < n; i++ )
                    m_pixels[i * 4] = a[i];
                free(a);
            }
        }
        else
        {
            wxOL_CGX_READ(m_pixels, 0, 0, m_width * 4, &rp, 0, 0, m_width, m_height);
            ULONG *p = (ULONG *)m_pixels;
            for ( size_t i = 0, n = (size_t)m_width * m_height; i < n; i++ )
                p[i] |= 0xff000000UL;
        }
    }

    int m_width, m_height, m_depth;
    unsigned char *m_pixels;
    bool m_alpha;
    wxMask *m_mask;
#if wxUSE_PALETTE
    wxPalette m_palette;
#endif
    struct BitMap *m_bm;
    bool m_bmValid;     // the BitMap holds the pixels
    bool m_bmNewer;     // and something was drawn in it since
};

#define M_BMPDATA ((wxBitmapRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmap, wxGDIObject);

// the BitMap was drawn on (a memory DC): the pixels are read back from it
void wxOLBitmapDrawnOn(wxBitmap& bmp)
{
    if ( bmp.IsOk() )
        ((wxBitmapRefData *)bmp.GetRefData())->m_bmNewer = true;
}

bool wxBitmap::Create(int width, int height, int depth)
{
    UnRef();
    if ( width <= 0 || height <= 0 )
        return false;
    m_refData = new wxBitmapRefData(width, height, depth == wxBITMAP_SCREEN_DEPTH ? 24 : depth);
    if ( depth == 32 )
        M_BMPDATA->m_alpha = true;
    return M_BMPDATA->IsOk();
}

wxBitmap::wxBitmap(const char bits[], int width, int height, int depth)
{
    wxCHECK_RET(depth == 1, "only 1 bit a pixel XBM data");
    if ( !Create(width, height, 1) )
        return;
    // XBM: bit set is the foreground, black
    ULONG *p = (ULONG *)M_BMPDATA->m_pixels;
    const int bpr = (width + 7) / 8;
    for ( int y = 0; y < height; y++ )
        for ( int x = 0; x < width; x++ )
        {
            bool on = (bits[y * bpr + x / 8] >> (x & 7)) & 1;
            p[y * width + x] = on ? 0xff000000UL : 0xffffffffUL;
        }
}

wxBitmap::wxBitmap(const wxString& filename, wxBitmapType type)
{
    LoadFile(filename, type);
}

// wxBitmap(const char* const*), from XPM: wx's common code (bmpbase.cpp)

#if wxUSE_IMAGE
void wxBitmap::InitFromImage(const wxImage& image, int depth, double scale)
{
    wxCHECK_RET(image.IsOk(), "invalid image");
    const int w = image.GetWidth(), h = image.GetHeight();
    if ( !Create(w, h, depth == 1 ? 1 : 24) )
        return;
    SetScaleFactor(scale);
    unsigned char *p = M_BMPDATA->m_pixels;
    const unsigned char *rgb = image.GetData(), *alpha = image.HasAlpha() ? image.GetAlpha() : NULL;
    const bool mask = image.HasMask();
    unsigned char mr = 0, mg = 0, mb = 0;
    if ( mask )
        image.GetOrFindMaskColour(&mr, &mg, &mb);
    for ( int i = 0, n = w * h; i < n; i++, p += 4, rgb += 3 )
    {
        unsigned char a = alpha ? alpha[i] : 0xff;
        if ( mask && rgb[0] == mr && rgb[1] == mg && rgb[2] == mb )
            a = 0;
        if ( depth == 1 )
        {
            // a mono bitmap: white or black
            const unsigned char v = (rgb[0] + rgb[1] + rgb[2]) >= 384 ? 0xff : 0;
            p[0] = 0xff; p[1] = p[2] = p[3] = v;
            continue;
        }
        p[0] = a;
        p[1] = rgb[0];
        p[2] = rgb[1];
        p[3] = rgb[2];
    }
    // an image's mask becomes the alpha the DC draws by
    M_BMPDATA->m_alpha = alpha != NULL || mask;
}

wxImage wxBitmap::ConvertToImage() const
{
    wxCHECK_MSG(IsOk(), wxNullImage, "invalid bitmap");
    M_BMPDATA->Sync();
    const int w = GetWidth(), h = GetHeight();
    wxImage img(w, h, false);
    unsigned char *rgb = img.GetData();
    unsigned char *alpha = NULL;
    if ( M_BMPDATA->m_alpha )
    {
        img.SetAlpha();
        alpha = img.GetAlpha();
    }
    const unsigned char *p = M_BMPDATA->m_pixels;
    for ( int i = 0, n = w * h; i < n; i++, p += 4, rgb += 3 )
    {
        rgb[0] = p[1];
        rgb[1] = p[2];
        rgb[2] = p[3];
        if ( alpha )
            alpha[i] = p[0];
    }
    if ( M_BMPDATA->m_mask )
    {
        // white in the mask is shown, black is not
        wxBitmap mb = M_BMPDATA->m_mask->GetBitmap();
        if ( mb.IsOk() && mb.GetWidth() == w && mb.GetHeight() == h )
        {
            const unsigned char *m = mb.GetPixels();
            rgb = img.GetData();
            unsigned char kr = 1, kg = 2, kb = 3;
            img.SetMaskColour(kr, kg, kb);
            for ( int i = 0, n = w * h; i < n; i++, m += 4 )
                if ( !m[1] )
                {
                    rgb[i * 3] = kr; rgb[i * 3 + 1] = kg; rgb[i * 3 + 2] = kb;
                }
        }
    }
    return img;
}
#endif // wxUSE_IMAGE

int wxBitmap::GetHeight() const { return IsOk() ? M_BMPDATA->m_height : -1; }
int wxBitmap::GetWidth() const { return IsOk() ? M_BMPDATA->m_width : -1; }
int wxBitmap::GetDepth() const
{
    if ( !IsOk() )
        return -1;
    return M_BMPDATA->m_alpha ? 32 : M_BMPDATA->m_depth;
}

wxMask *wxBitmap::GetMask() const { return IsOk() ? M_BMPDATA->m_mask : NULL; }

void wxBitmap::SetMask(wxMask *mask)
{
    wxCHECK_RET(IsOk(), "invalid bitmap");
    AllocExclusive();
    delete M_BMPDATA->m_mask;
    M_BMPDATA->m_mask = mask;
}

wxBitmap wxBitmap::GetSubBitmap(const wxRect& rect) const
{
    wxCHECK_MSG(IsOk(), wxNullBitmap, "invalid bitmap");
    wxRect r = rect.Intersect(wxRect(0, 0, GetWidth(), GetHeight()));
    wxCHECK_MSG(!r.IsEmpty(), wxNullBitmap, "invalid rectangle");
    M_BMPDATA->Sync();
    wxBitmap sub(r.width, r.height, GetDepth());
    wxBitmapRefData *d = (wxBitmapRefData *)sub.GetRefData();
    d->m_alpha = M_BMPDATA->m_alpha;
    for ( int y = 0; y < r.height; y++ )
        memcpy(d->m_pixels + (size_t)y * r.width * 4,
               M_BMPDATA->m_pixels + ((size_t)(r.y + y) * M_BMPDATA->m_width + r.x) * 4, (size_t)r.width * 4);
    if ( M_BMPDATA->m_mask )
    {
        wxBitmap mb = M_BMPDATA->m_mask->GetBitmap();
        if ( mb.IsOk() )
            sub.SetMask(new wxMask(mb.GetSubBitmap(r)));
    }
    return sub;
}

bool wxBitmap::SaveFile(const wxString& name, wxBitmapType type, const wxPalette *WXUNUSED(palette)) const
{
#if wxUSE_IMAGE
    return IsOk() && ConvertToImage().SaveFile(name, type);
#else
    return false;
#endif
}

bool wxBitmap::LoadFile(const wxString& name, wxBitmapType type)
{
#if wxUSE_IMAGE
    wxImage img;
    if ( !img.LoadFile(name, type) )
        return false;
    InitFromImage(img, wxBITMAP_SCREEN_DEPTH, 1.0);
    return IsOk();
#else
    wxUnusedVar(name);
    wxUnusedVar(type);
    return false;
#endif
}

#if wxUSE_PALETTE
wxPalette *wxBitmap::GetPalette() const { return IsOk() ? &M_BMPDATA->m_palette : NULL; }
void wxBitmap::SetPalette(const wxPalette& palette)
{
    wxCHECK_RET(IsOk(), "invalid bitmap");
    AllocExclusive();
    M_BMPDATA->m_palette = palette;
}
#endif

void wxBitmap::InitStandardHandlers()
{
}

void *wxBitmap::GetRawData(wxPixelDataBase& data, int bpp)
{
    if ( !IsOk() || bpp != 32 )
        return NULL;
    AllocExclusive();
    M_BMPDATA->Sync();
    // the pixels are the caller's now: the BitMap is written again from them
    M_BMPDATA->m_bmValid = false;
    data.m_width = M_BMPDATA->m_width;
    data.m_height = M_BMPDATA->m_height;
    data.m_stride = M_BMPDATA->m_width * 4;
    return M_BMPDATA->m_pixels;
}

void wxBitmap::UngetRawData(wxPixelDataBase& WXUNUSED(data))
{
    if ( IsOk() )
        M_BMPDATA->m_bmValid = false;
}

bool wxBitmap::HasAlpha() const
{
    return IsOk() && M_BMPDATA->m_alpha;
}

bool wxBitmap::UseAlpha(bool use)
{
    if ( !IsOk() )
        return false;
    if ( M_BMPDATA->m_alpha != use )
    {
        AllocExclusive();
        M_BMPDATA->m_alpha = use;
    }
    return true;
}

#if WXWIN_COMPATIBILITY_3_0
void wxBitmap::SetHeight(int height)
{
    if ( IsOk() && height != GetHeight() )
    {
        wxBitmap b(GetWidth(), height, GetDepth());
        *this = b;
    }
}
void wxBitmap::SetWidth(int width)
{
    if ( IsOk() && width != GetWidth() )
    {
        wxBitmap b(width, GetHeight(), GetDepth());
        *this = b;
    }
}
void wxBitmap::SetDepth(int depth)
{
    if ( IsOk() )
    {
        AllocExclusive();
        M_BMPDATA->m_depth = depth;
        M_BMPDATA->m_alpha = depth == 32;
    }
}
#endif

unsigned char *wxBitmap::GetPixels()
{
    if ( !IsOk() )
        return NULL;
    AllocExclusive();
    M_BMPDATA->Sync();
    M_BMPDATA->m_bmValid = false;
    return M_BMPDATA->m_pixels;
}

const unsigned char *wxBitmap::GetPixels() const
{
    if ( !IsOk() )
        return NULL;
    M_BMPDATA->Sync();
    return M_BMPDATA->m_pixels;
}

struct BitMap *wxBitmap::GetBitMap()
{
    return IsOk() ? M_BMPDATA->BitMap_() : NULL;
}

void wxBitmap::Sync()
{
    if ( IsOk() )
        M_BMPDATA->Sync();
}

void wxBitmap::Unshare()
{
    if ( IsOk() )
    {
        M_BMPDATA->Sync();
        M_BMPDATA->FreeBitMap_();
    }
}

wxGDIRefData *wxBitmap::CreateGDIRefData() const
{
    return new wxBitmapRefData(1, 1, 24);
}

wxGDIRefData *wxBitmap::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxBitmapRefData(*(const wxBitmapRefData *)data);
}
