/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/stattext.h
// Purpose:     wxStaticText: an olui paragraph
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_STATTEXT_H_
#define _WX_OPENLAYOUT_STATTEXT_H_

class WXDLLIMPEXP_CORE wxStaticText : public wxStaticTextBase
{
public:
    wxStaticText() {}
    wxStaticText(wxWindow *parent, wxWindowID id, const wxString& label,
                 const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                 long style = 0, const wxString& name = wxASCII_STR(wxStaticTextNameStr))
    {
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxString& name = wxASCII_STR(wxStaticTextNameStr));

    virtual void SetLabel(const wxString& label) wxOVERRIDE;
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual wxString WXGetVisibleLabel() const wxOVERRIDE { return m_olVisible; }
    virtual void WXSetVisibleLabel(const wxString& str) wxOVERRIDE;

private:
    wxString m_olVisible;

    wxDECLARE_DYNAMIC_CLASS(wxStaticText);
    wxDECLARE_NO_COPY_CLASS(wxStaticText);
};

#endif // _WX_OPENLAYOUT_STATTEXT_H_
