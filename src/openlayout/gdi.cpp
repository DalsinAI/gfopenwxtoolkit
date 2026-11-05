/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/gdi.cpp
// Purpose:     wxPen, wxBrush and wxCursor: what the DC and the windows
//              read when they draw and point
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/cursor.h"
    #include "wx/bitmap.h"
    #include "wx/image.h"
#endif

// ---- wxPen ----------------------------------------------------------------------

class wxPenRefData : public wxGDIRefData
{
public:
    wxPenRefData()
        : m_width(1), m_style(wxPENSTYLE_SOLID), m_cap(wxCAP_ROUND), m_join(wxJOIN_ROUND),
          m_dashes(NULL), m_nDashes(0) {}
    wxPenRefData(const wxPenRefData& d)
        : wxGDIRefData(), m_colour(d.m_colour), m_width(d.m_width), m_style(d.m_style), m_cap(d.m_cap),
          m_join(d.m_join), m_dashes(d.m_dashes), m_nDashes(d.m_nDashes), m_stipple(d.m_stipple) {}

    bool operator==(const wxPenRefData& d) const
    {
        return m_colour == d.m_colour && m_width == d.m_width && m_style == d.m_style && m_cap == d.m_cap &&
               m_join == d.m_join && m_nDashes == d.m_nDashes && m_dashes == d.m_dashes;
    }

    wxColour m_colour;
    int m_width;
    wxPenStyle m_style;
    wxPenCap m_cap;
    wxPenJoin m_join;
    wxDash *m_dashes;       // the caller's, as wx's other ports keep them
    int m_nDashes;
    wxBitmap m_stipple;
};

#define M_PENDATA ((wxPenRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxPen, wxGDIObject);

wxPen::wxPen(const wxColour& colour, int width, wxPenStyle style)
{
    m_refData = new wxPenRefData;
    M_PENDATA->m_colour = colour;
    M_PENDATA->m_width = width;
    M_PENDATA->m_style = style;
}

wxPen::wxPen(const wxColour& colour, int width, int style)
{
    m_refData = new wxPenRefData;
    M_PENDATA->m_colour = colour;
    M_PENDATA->m_width = width;
    M_PENDATA->m_style = (wxPenStyle)style;
}

wxPen::wxPen(const wxPenInfo& info)
{
    m_refData = new wxPenRefData;
    M_PENDATA->m_colour = info.GetColour();
    M_PENDATA->m_width = info.GetWidth();
    M_PENDATA->m_style = info.GetStyle();
    M_PENDATA->m_cap = info.GetCap();
    M_PENDATA->m_join = info.GetJoin();
    M_PENDATA->m_stipple = info.GetStipple();
    wxDash *dash;
    M_PENDATA->m_nDashes = info.GetDashes(&dash);
    M_PENDATA->m_dashes = dash;
}

wxPen::wxPen(const wxBitmap& stipple, int width)
{
    m_refData = new wxPenRefData;
    M_PENDATA->m_stipple = stipple;
    M_PENDATA->m_width = width;
    M_PENDATA->m_style = wxPENSTYLE_STIPPLE;
}

bool wxPen::operator==(const wxPen& pen) const
{
    if ( m_refData == pen.m_refData )
        return true;
    if ( !m_refData || !pen.m_refData )
        return false;
    return *M_PENDATA == *(wxPenRefData *)pen.m_refData;
}

void wxPen::SetColour(const wxColour& colour) { AllocExclusive(); M_PENDATA->m_colour = colour; }
void wxPen::SetColour(unsigned char r, unsigned char g, unsigned char b) { SetColour(wxColour(r, g, b)); }
void wxPen::SetCap(wxPenCap cap) { AllocExclusive(); M_PENDATA->m_cap = cap; }
void wxPen::SetJoin(wxPenJoin join) { AllocExclusive(); M_PENDATA->m_join = join; }
void wxPen::SetStyle(wxPenStyle style) { AllocExclusive(); M_PENDATA->m_style = style; }
void wxPen::SetWidth(int width) { AllocExclusive(); M_PENDATA->m_width = width; }
void wxPen::SetDashes(int n, const wxDash *dash)
{
    AllocExclusive();
    M_PENDATA->m_nDashes = n;
    M_PENDATA->m_dashes = (wxDash *)dash;
    M_PENDATA->m_style = wxPENSTYLE_USER_DASH;
}
void wxPen::SetStipple(const wxBitmap& stipple)
{
    AllocExclusive();
    M_PENDATA->m_stipple = stipple;
    M_PENDATA->m_style = wxPENSTYLE_STIPPLE;
}

wxColour wxPen::GetColour() const { wxCHECK_MSG(IsOk(), wxNullColour, "invalid pen"); return M_PENDATA->m_colour; }
wxPenCap wxPen::GetCap() const { wxCHECK_MSG(IsOk(), wxCAP_INVALID, "invalid pen"); return M_PENDATA->m_cap; }
wxPenJoin wxPen::GetJoin() const { wxCHECK_MSG(IsOk(), wxJOIN_INVALID, "invalid pen"); return M_PENDATA->m_join; }
wxPenStyle wxPen::GetStyle() const { wxCHECK_MSG(IsOk(), wxPENSTYLE_INVALID, "invalid pen"); return M_PENDATA->m_style; }
int wxPen::GetWidth() const { wxCHECK_MSG(IsOk(), -1, "invalid pen"); return M_PENDATA->m_width; }
int wxPen::GetDashes(wxDash **ptr) const
{
    wxCHECK_MSG(IsOk(), -1, "invalid pen");
    *ptr = M_PENDATA->m_dashes;
    return M_PENDATA->m_nDashes;
}
int wxPen::GetDashCount() const { return IsOk() ? M_PENDATA->m_nDashes : 0; }
wxDash *wxPen::GetDash() const { return IsOk() ? M_PENDATA->m_dashes : NULL; }
wxBitmap *wxPen::GetStipple() const { wxCHECK_MSG(IsOk(), NULL, "invalid pen"); return &M_PENDATA->m_stipple; }

wxGDIRefData *wxPen::CreateGDIRefData() const { return new wxPenRefData; }
wxGDIRefData *wxPen::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxPenRefData(*(const wxPenRefData *)data);
}

// ---- wxBrush ----------------------------------------------------------------------

class wxBrushRefData : public wxGDIRefData
{
public:
    wxBrushRefData() : m_style(wxBRUSHSTYLE_SOLID) {}
    wxBrushRefData(const wxBrushRefData& d)
        : wxGDIRefData(), m_colour(d.m_colour), m_style(d.m_style), m_stipple(d.m_stipple) {}

    wxColour m_colour;
    wxBrushStyle m_style;
    wxBitmap m_stipple;
};

#define M_BRUSHDATA ((wxBrushRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxBrush, wxGDIObject);

wxBrush::wxBrush(const wxColour& colour, wxBrushStyle style)
{
    m_refData = new wxBrushRefData;
    M_BRUSHDATA->m_colour = colour;
    M_BRUSHDATA->m_style = style;
}

wxBrush::wxBrush(const wxColour& colour, int style)
{
    m_refData = new wxBrushRefData;
    M_BRUSHDATA->m_colour = colour;
    M_BRUSHDATA->m_style = (wxBrushStyle)style;
}

wxBrush::wxBrush(const wxBitmap& stipple)
{
    m_refData = new wxBrushRefData;
    M_BRUSHDATA->m_stipple = stipple;
    M_BRUSHDATA->m_style = stipple.GetMask() ? wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE : wxBRUSHSTYLE_STIPPLE;
}

bool wxBrush::operator==(const wxBrush& brush) const
{
    if ( m_refData == brush.m_refData )
        return true;
    if ( !m_refData || !brush.m_refData )
        return false;
    const wxBrushRefData *o = (const wxBrushRefData *)brush.m_refData;
    return M_BRUSHDATA->m_colour == o->m_colour && M_BRUSHDATA->m_style == o->m_style &&
           M_BRUSHDATA->m_stipple.IsSameAs(o->m_stipple);
}

wxBrushStyle wxBrush::GetStyle() const { wxCHECK_MSG(IsOk(), wxBRUSHSTYLE_INVALID, "invalid brush"); return M_BRUSHDATA->m_style; }
wxColour wxBrush::GetColour() const { wxCHECK_MSG(IsOk(), wxNullColour, "invalid brush"); return M_BRUSHDATA->m_colour; }
wxBitmap *wxBrush::GetStipple() const { wxCHECK_MSG(IsOk(), NULL, "invalid brush"); return &M_BRUSHDATA->m_stipple; }
void wxBrush::SetColour(const wxColour& col) { AllocExclusive(); M_BRUSHDATA->m_colour = col; }
void wxBrush::SetColour(unsigned char r, unsigned char g, unsigned char b) { SetColour(wxColour(r, g, b)); }
void wxBrush::SetStyle(wxBrushStyle style) { AllocExclusive(); M_BRUSHDATA->m_style = style; }
void wxBrush::SetStipple(const wxBitmap& stipple)
{
    AllocExclusive();
    M_BRUSHDATA->m_stipple = stipple;
    M_BRUSHDATA->m_style = stipple.GetMask() ? wxBRUSHSTYLE_STIPPLE_MASK_OPAQUE : wxBRUSHSTYLE_STIPPLE;
}

wxGDIRefData *wxBrush::CreateGDIRefData() const { return new wxBrushRefData; }
wxGDIRefData *wxBrush::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxBrushRefData(*(const wxBrushRefData *)data);
}

// ---- wxCursor -----------------------------------------------------------------------

class wxCursorRefData : public wxGDIRefData
{
public:
    wxCursorRefData() : m_stock(wxCURSOR_NONE) {}
    wxStockCursor m_stock;
#if wxUSE_IMAGE
    wxImage m_image;
#endif
};

#define M_CURSORDATA ((wxCursorRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxCursor, wxGDIObject);

void wxCursor::InitFromStock(wxStockCursor id)
{
    UnRef();
    m_refData = new wxCursorRefData;
    M_CURSORDATA->m_stock = id;
}

#if wxUSE_IMAGE
wxCursor::wxCursor(const wxImage& image)
{
    m_refData = new wxCursorRefData;
    M_CURSORDATA->m_image = image;
}

wxCursor::wxCursor(const char* const* xpmData)
{
    m_refData = new wxCursorRefData;
    M_CURSORDATA->m_image = wxImage(xpmData);
}
#endif

wxCursor::wxCursor(const wxString& name, wxBitmapType type, int WXUNUSED(hotSpotX), int WXUNUSED(hotSpotY))
{
    m_refData = new wxCursorRefData;
#if wxUSE_IMAGE
    M_CURSORDATA->m_image.LoadFile(name, type);
#else
    wxUnusedVar(name);
    wxUnusedVar(type);
#endif
}

wxStockCursor wxCursor::GetStockId() const
{
    return IsOk() ? M_CURSORDATA->m_stock : wxCURSOR_NONE;
}

wxGDIRefData *wxCursor::CreateGDIRefData() const { return new wxCursorRefData; }
wxGDIRefData *wxCursor::CloneGDIRefData(const wxGDIRefData *data) const
{
    wxCursorRefData *d = new wxCursorRefData;
    d->m_stock = ((const wxCursorRefData *)data)->m_stock;
#if wxUSE_IMAGE
    d->m_image = ((const wxCursorRefData *)data)->m_image;
#endif
    return d;
}

void wxSetCursor(const wxCursor& WXUNUSED(cursor))
{
    // the pointer is each window's (wxWindow::SetCursor)
}
