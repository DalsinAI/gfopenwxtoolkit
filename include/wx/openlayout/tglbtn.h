/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/tglbtn.h
// Purpose:     wxToggleButton
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TGLBTN_H_
#define _WX_OPENLAYOUT_TGLBTN_H_

class WXDLLIMPEXP_CORE wxToggleButton : public wxToggleButtonBase
{
public:
    wxToggleButton() : m_olValue(false) {}
    wxToggleButton(wxWindow *parent, wxWindowID id, const wxString& label,
                   const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                   long style = 0, const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxASCII_STR(wxCheckBoxNameStr))
        : m_olValue(false)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxCheckBoxNameStr));

    virtual void SetValue(bool state) wxOVERRIDE;
    virtual bool GetValue() const wxOVERRIDE { return m_olValue; }
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual wxString OLGetMarkup() const wxOVERRIDE;

private:
    bool m_olValue;

    wxDECLARE_DYNAMIC_CLASS(wxToggleButton);
    wxDECLARE_NO_COPY_CLASS(wxToggleButton);
};

class WXDLLIMPEXP_CORE wxBitmapToggleButton : public wxToggleButton
{
public:
    wxBitmapToggleButton() {}
    wxBitmapToggleButton(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label,
                         const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                         long style = 0, const wxValidator& validator = wxDefaultValidator,
                         const wxString& name = wxASCII_STR(wxCheckBoxNameStr))
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxCheckBoxNameStr));

    wxDECLARE_DYNAMIC_CLASS(wxBitmapToggleButton);
    wxDECLARE_NO_COPY_CLASS(wxBitmapToggleButton);
};

#endif // _WX_OPENLAYOUT_TGLBTN_H_
