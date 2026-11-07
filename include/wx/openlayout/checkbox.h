/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/checkbox.h
// Purpose:     wxCheckBox: an olui checkbox
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CHECKBOX_H_
#define _WX_OPENLAYOUT_CHECKBOX_H_

class WXDLLIMPEXP_CORE wxCheckBox : public wxCheckBoxBase
{
public:
    wxCheckBox() : m_olState(wxCHK_UNCHECKED) {}
    wxCheckBox(wxWindow *parent, wxWindowID id, const wxString& label,
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = 0, const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxASCII_STR(wxCheckBoxNameStr))
        : m_olState(wxCHK_UNCHECKED)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& label,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxCheckBoxNameStr));

    virtual void SetValue(bool value) wxOVERRIDE;
    virtual bool GetValue() const wxOVERRIDE { return m_olState == wxCHK_CHECKED; }
    virtual void SetLabel(const wxString& label) wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

protected:
    // the label on one line, with olui's room for the box at its start
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual void DoSet3StateValue(wxCheckBoxState state) wxOVERRIDE;
    virtual wxCheckBoxState DoGet3StateValue() const wxOVERRIDE { return m_olState; }

private:
    wxCheckBoxState m_olState;

    wxDECLARE_DYNAMIC_CLASS(wxCheckBox);
    wxDECLARE_NO_COPY_CLASS(wxCheckBox);
};

#endif // _WX_OPENLAYOUT_CHECKBOX_H_
