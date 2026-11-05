/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/bmpbuttn.h
// Purpose:     wxBitmapButton
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_BMPBUTTN_H_
#define _WX_OPENLAYOUT_BMPBUTTN_H_

class WXDLLIMPEXP_CORE wxBitmapButton : public wxBitmapButtonBase
{
public:
    wxBitmapButton() {}
    wxBitmapButton(wxWindow *parent, wxWindowID id, const wxBitmapBundle& bitmap,
                   const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                   long style = 0, const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxASCII_STR(wxButtonNameStr))
    {
        Create(parent, id, bitmap, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& bitmap,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxButtonNameStr));

    bool CreateCloseButton(wxWindow *parent, wxWindowID winid,
                           const wxString& name = wxString());

    wxDECLARE_DYNAMIC_CLASS(wxBitmapButton);
    wxDECLARE_NO_COPY_CLASS(wxBitmapButton);
};

#endif // _WX_OPENLAYOUT_BMPBUTTN_H_
