/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/statline.h
// Purpose:     wxStaticLine: an olui hr
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_STATLINE_H_
#define _WX_OPENLAYOUT_STATLINE_H_

class WXDLLIMPEXP_CORE wxStaticLine : public wxStaticLineBase
{
public:
    wxStaticLine(){}
    wxStaticLine(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxLI_HORIZONTAL, const wxString& name = wxASCII_STR(wxStaticLineNameStr))
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxLI_HORIZONTAL, const wxString& name = wxASCII_STR(wxStaticLineNameStr));

    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

    wxDECLARE_DYNAMIC_CLASS(wxStaticLine);
    wxDECLARE_NO_COPY_CLASS(wxStaticLine);
};

#endif // _WX_OPENLAYOUT_STATLINE_H_
