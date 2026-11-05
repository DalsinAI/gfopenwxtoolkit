/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/statbox.h
// Purpose:     wxStaticBox: an olui fieldset
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_STATBOX_H_
#define _WX_OPENLAYOUT_STATBOX_H_

class WXDLLIMPEXP_CORE wxStaticBox : public wxStaticBoxBase
{
public:
    wxStaticBox() {}
    wxStaticBox(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxString& name = wxASCII_STR(wxStaticBoxNameStr))
    {
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxString& name = wxASCII_STR(wxStaticBoxNameStr));

    virtual void GetBordersForSizer(int *borderTop, int *borderOther) const wxOVERRIDE;
    virtual void SetLabel(const wxString& label) wxOVERRIDE;
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

    wxDECLARE_DYNAMIC_CLASS(wxStaticBox);
};

#endif // _WX_OPENLAYOUT_STATBOX_H_
