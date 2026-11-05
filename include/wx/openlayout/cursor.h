/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/cursor.h
// Purpose:     wxCursor: Intuition pointers
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CURSOR_H_
#define _WX_OPENLAYOUT_CURSOR_H_

class WXDLLIMPEXP_FWD_CORE wxImage;

// A stock cursor (the busy pointer through WA_BusyPointer, the rest the
// Workbench's pointer for now) or one made from an image.
class WXDLLIMPEXP_CORE wxCursor : public wxCursorBase
{
public:
    wxCursor() {}
    wxCursor(wxStockCursor id) { InitFromStock(id); }
#if WXWIN_COMPATIBILITY_2_8
    wxCursor(int id) { InitFromStock((wxStockCursor)id); }
#endif
#if wxUSE_IMAGE
    wxCursor(const wxImage& image);
    wxCursor(const char* const* xpmData);
#endif
    wxCursor(const wxString& name, wxBitmapType type = wxCURSOR_DEFAULT_TYPE,
             int hotSpotX = 0, int hotSpotY = 0);

    // which stock pointer it is (wxCURSOR_NONE: none, or made from an image)
    wxStockCursor GetStockId() const;

protected:
    void InitFromStock(wxStockCursor);

    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxCursor);
};

#endif // _WX_OPENLAYOUT_CURSOR_H_
