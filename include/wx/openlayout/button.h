/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/button.h
// Purpose:     wxButton
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_BUTTON_H_
#define _WX_OPENLAYOUT_BUTTON_H_

class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() {}
    wxButton(wxWindow *parent, wxWindowID id, const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
             long style = 0, const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxASCII_STR(wxButtonNameStr))
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxButtonNameStr));

    virtual wxWindow *SetDefault() wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxButton);
    wxDECLARE_NO_COPY_CLASS(wxButton);
};

#endif // _WX_OPENLAYOUT_BUTTON_H_
