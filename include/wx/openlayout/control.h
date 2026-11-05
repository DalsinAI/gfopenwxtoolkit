/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/control.h
// Purpose:     wxControl
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CONTROL_H_
#define _WX_OPENLAYOUT_CONTROL_H_

class WXDLLIMPEXP_CORE wxControl : public wxControlBase
{
public:
    wxControl() {}
    wxControl(wxWindow *parent, wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize, long style = 0,
              const wxValidator& validator = wxDefaultValidator,
              const wxString& name = wxASCII_STR(wxControlNameStr))
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxControlNameStr));

    virtual wxVisualAttributes GetDefaultAttributes() const wxOVERRIDE
        { return GetClassDefaultAttributes(GetWindowVariant()); }
    static wxVisualAttributes GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    // a native control's best size: its widget's text and the theme's
    // padding (olui measures it)
    virtual wxSize DoGetBestSize() const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxControl);
    wxDECLARE_NO_COPY_CLASS(wxControl);
};

#endif // _WX_OPENLAYOUT_CONTROL_H_
