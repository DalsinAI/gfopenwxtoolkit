/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/radiobox.h
// Purpose:     wxRadioBox: an olui fieldset of radio buttons
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_RADIOBOX_H_
#define _WX_OPENLAYOUT_RADIOBOX_H_

class WXDLLIMPEXP_CORE wxRadioBox : public wxControl, public wxRadioBoxBase
{
public:
    wxRadioBox() : m_olSel(wxNOT_FOUND) {}
    wxRadioBox(wxWindow *parent, wxWindowID id, const wxString& title,
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               int n = 0, const wxString choices[] = NULL, int majorDim = 0,
               long style = wxRA_SPECIFY_COLS, const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxASCII_STR(wxRadioBoxNameStr))
        : m_olSel(wxNOT_FOUND)
    {
        Create(parent, id, title, pos, size, n, choices, majorDim, style, val, name);
    }
    wxRadioBox(wxWindow *parent, wxWindowID id, const wxString& title,
               const wxPoint& pos, const wxSize& size, const wxArrayString& choices,
               int majorDim = 0, long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxASCII_STR(wxRadioBoxNameStr))
        : m_olSel(wxNOT_FOUND)
    {
        Create(parent, id, title, pos, size, choices, majorDim, style, val, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& title,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL, int majorDim = 0,
                long style = wxRA_SPECIFY_COLS, const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxRadioBoxNameStr));
    bool Create(wxWindow *parent, wxWindowID id, const wxString& title,
                const wxPoint& pos, const wxSize& size, const wxArrayString& choices,
                int majorDim = 0, long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxRadioBoxNameStr));

    using wxWindowBase::Show;
    using wxWindowBase::Enable;
    using wxRadioBoxBase::GetDefaultBorder;

    virtual bool Enable(unsigned int n, bool enable = true) wxOVERRIDE;
    virtual bool Enable(bool enable = true) wxOVERRIDE;
    virtual bool Show(unsigned int n, bool show = true) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual bool IsItemEnabled(unsigned int n) const wxOVERRIDE;
    virtual bool IsItemShown(unsigned int n) const wxOVERRIDE;
    virtual unsigned int GetCount() const wxOVERRIDE { return (unsigned int)m_olItems.size(); }
    virtual wxString GetString(unsigned int n) const wxOVERRIDE { return m_olItems[n]; }
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;
    virtual void SetSelection(int n) wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE { return m_olSel; }

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;

private:
    wxArrayString m_olItems;
    wxVector<bool> m_olEnabled, m_olShown;
    int m_olSel;

    wxDECLARE_DYNAMIC_CLASS(wxRadioBox);
    wxDECLARE_NO_COPY_CLASS(wxRadioBox);
};

#endif // _WX_OPENLAYOUT_RADIOBOX_H_
