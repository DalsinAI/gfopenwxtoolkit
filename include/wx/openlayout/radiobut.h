/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/radiobut.h
// Purpose:     wxRadioButton: an olui radio button
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_RADIOBUT_H_
#define _WX_OPENLAYOUT_RADIOBUT_H_

class WXDLLIMPEXP_CORE wxRadioButton : public wxRadioButtonBase
{
public:
    wxRadioButton() : m_olValue(false) {}
    wxRadioButton(wxWindow *parent, wxWindowID id, const wxString& label,
                  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                  long style = 0, const wxValidator& validator = wxDefaultValidator,
                  const wxString& name = wxASCII_STR(wxRadioButtonNameStr))
        : m_olValue(false)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxRadioButtonNameStr));

    virtual void SetValue(bool value) wxOVERRIDE;
    virtual bool GetValue() const wxOVERRIDE { return m_olValue; }
    virtual void SetLabel(const wxString& label) wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

private:
    bool m_olValue;

    wxDECLARE_DYNAMIC_CLASS(wxRadioButton);
    wxDECLARE_NO_COPY_CLASS(wxRadioButton);
};

#endif // _WX_OPENLAYOUT_RADIOBUT_H_
