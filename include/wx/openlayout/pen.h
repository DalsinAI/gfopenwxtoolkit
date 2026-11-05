/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/pen.h
// Purpose:     wxPen
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_PEN_H_
#define _WX_OPENLAYOUT_PEN_H_

class WXDLLIMPEXP_FWD_CORE wxBitmap;

// A pen is a colour, a width, a style and dashes: the DC turns it into a
// graphics.library pen (or a cybergraphics colour) when it draws.
class WXDLLIMPEXP_CORE wxPen : public wxPenBase
{
public:
    wxPen() {}
    wxPen(const wxColour& colour, int width = 1, wxPenStyle style = wxPENSTYLE_SOLID);
    wxPen(const wxPenInfo& info);
    wxPen(const wxBitmap& stipple, int width);

    bool operator==(const wxPen& pen) const;
    bool operator!=(const wxPen& pen) const { return !(*this == pen); }

    virtual void SetColour(const wxColour& colour) wxOVERRIDE;
    virtual void SetColour(unsigned char r, unsigned char g, unsigned char b) wxOVERRIDE;
    virtual void SetCap(wxPenCap cap) wxOVERRIDE;
    virtual void SetJoin(wxPenJoin join) wxOVERRIDE;
    virtual void SetStyle(wxPenStyle style) wxOVERRIDE;
    virtual void SetWidth(int width) wxOVERRIDE;
    virtual void SetDashes(int n, const wxDash *dash) wxOVERRIDE;
    virtual void SetStipple(const wxBitmap& stipple) wxOVERRIDE;

    virtual wxColour GetColour() const wxOVERRIDE;
    virtual wxPenCap GetCap() const wxOVERRIDE;
    virtual wxPenJoin GetJoin() const wxOVERRIDE;
    virtual wxPenStyle GetStyle() const wxOVERRIDE;
    virtual int GetWidth() const wxOVERRIDE;
    virtual int GetDashes(wxDash **ptr) const wxOVERRIDE;
    virtual wxBitmap *GetStipple() const wxOVERRIDE;
    int GetDashCount() const;
    wxDash *GetDash() const;

    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    wxPen(const wxColour& col, int width, int style);
    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    void SetStyle(int style) { SetStyle((wxPenStyle)style); }

protected:
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxPen);
};

#endif // _WX_OPENLAYOUT_PEN_H_
