/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/statbmp.h
// Purpose:     wxStaticBitmap: an olui img the port draws
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_STATBMP_H_
#define _WX_OPENLAYOUT_STATBMP_H_

class WXDLLIMPEXP_CORE wxStaticBitmap : public wxStaticBitmapBase
{
public:
    wxStaticBitmap(){}
    wxStaticBitmap(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const wxString& name = wxASCII_STR(wxStaticBitmapNameStr))
    {
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0, const wxString& name = wxASCII_STR(wxStaticBitmapNameStr));

    virtual void SetBitmap(const wxBitmapBundle& bitmap) wxOVERRIDE;
    virtual wxBitmap GetBitmap() const wxOVERRIDE;
private:
    // draws the bitmap centred, as a custom-drawn window
    void OnPaint(wxPaintEvent& event);
public:

    wxDECLARE_DYNAMIC_CLASS(wxStaticBitmap);
    wxDECLARE_NO_COPY_CLASS(wxStaticBitmap);
};

#endif // _WX_OPENLAYOUT_STATBMP_H_
