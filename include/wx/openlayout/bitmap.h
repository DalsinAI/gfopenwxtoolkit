/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/bitmap.h
// Purpose:     wxBitmap: ARGB pixels, and a screen BitMap while a DC draws on it
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_BITMAP_H_
#define _WX_OPENLAYOUT_BITMAP_H_

#include "wx/openlayout/ndktypes.h"

class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

// A bitmap's pixels are 4 bytes each, A R G B (cybergraphics' RECTFMT_ARGB),
// in memory of the program's own: raw access, wxImage and loading need
// nothing else. A wxMemoryDC drawing on it makes a BitMap like the screen's
// (GetBitMap) and gives it back to the pixels when it is done (Sync); a
// bitmap drawn to a window goes there with WritePixelArray, or from that
// BitMap while it has one.
class WXDLLIMPEXP_CORE wxBitmap : public wxBitmapBase
{
public:
    wxBitmap() {}
    wxBitmap(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH) { Create(width, height, depth); }
    wxBitmap(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH) { Create(sz, depth); }
    wxBitmap(int width, int height, const wxDC& dc) { Create(width, height, dc); }
    wxBitmap(const char bits[], int width, int height, int depth = 1);
    wxBitmap(const wxString& filename, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);
    wxBitmap(const char* const* bits);
#if wxUSE_IMAGE
    wxBitmap(const wxImage& image, int depth = wxBITMAP_SCREEN_DEPTH, double scale = 1.0)
        { InitFromImage(image, depth, scale); }
    wxBitmap(const wxImage& image, const wxDC& WXUNUSED(dc))
        { InitFromImage(image, wxBITMAP_SCREEN_DEPTH, 1.0); }
#endif

    bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }
    bool Create(int width, int height, const wxDC& WXUNUSED(dc))
        { return Create(width, height); }

    virtual int GetHeight() const wxOVERRIDE;
    virtual int GetWidth() const wxOVERRIDE;
    virtual int GetDepth() const wxOVERRIDE;

#if wxUSE_IMAGE
    virtual wxImage ConvertToImage() const wxOVERRIDE;
#endif

    virtual wxMask *GetMask() const wxOVERRIDE;
    virtual void SetMask(wxMask *mask) wxOVERRIDE;

    virtual wxBitmap GetSubBitmap(const wxRect& rect) const wxOVERRIDE;

    virtual bool SaveFile(const wxString& name, wxBitmapType type,
                          const wxPalette *palette = NULL) const wxOVERRIDE;
    virtual bool LoadFile(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE) wxOVERRIDE;

#if wxUSE_PALETTE
    virtual wxPalette *GetPalette() const wxOVERRIDE;
    virtual void SetPalette(const wxPalette& palette) wxOVERRIDE;
#endif

    static void InitStandardHandlers();

    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

    bool HasAlpha() const;
    virtual bool UseAlpha(bool use = true) wxOVERRIDE;

#if WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED(virtual void SetHeight(int height) wxOVERRIDE);
    wxDEPRECATED(virtual void SetWidth(int width) wxOVERRIDE);
    wxDEPRECATED(virtual void SetDepth(int depth) wxOVERRIDE);
#endif

    // the port's own: the pixels (ARGB, width * 4 bytes a row), written
    // through when the BitMap is synced back
    unsigned char *GetPixels();
    const unsigned char *GetPixels() const;
    // a BitMap like the screen's holding the pixels, made on the first call
    // (for a wxMemoryDC, and to blit from); Sync() reads it back into the
    // pixels and Unshare() drops it
    wxOL_BitMap *GetBitMap();
    void Sync();
    void Unshare();

protected:
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;

#if wxUSE_IMAGE
    void InitFromImage(const wxImage& image, int depth, double scale);
#endif

    wxDECLARE_DYNAMIC_CLASS(wxBitmap);
};

#endif // _WX_OPENLAYOUT_BITMAP_H_
