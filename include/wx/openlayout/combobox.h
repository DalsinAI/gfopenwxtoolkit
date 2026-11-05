/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/combobox.h
// Purpose:     wxComboBox: an olui input with a select's list
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_COMBOBOX_H_
#define _WX_OPENLAYOUT_COMBOBOX_H_

#include "wx/choice.h"
#include "wx/textentry.h"

class WXDLLIMPEXP_CORE wxComboBox : public wxChoice, public wxTextEntry
{
public:
    wxComboBox() {}
    wxComboBox(wxWindow *parent, wxWindowID id, const wxString& value = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxComboBoxNameStr))
    {
        Create(parent, id, value, pos, size, n, choices, style, validator, name);
    }
    wxComboBox(wxWindow *parent, wxWindowID id, const wxString& value, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxComboBoxNameStr))
    {
        Create(parent, id, value, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& value = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxComboBoxNameStr));
    bool Create(wxWindow *parent, wxWindowID id, const wxString& value, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxComboBoxNameStr));

    virtual void SetSelection(int n) wxOVERRIDE;
    virtual void SetSelection(long from, long to) wxOVERRIDE { wxTextEntry::SetSelection(from, to); }
    virtual int GetSelection() const wxOVERRIDE { return wxChoice::GetSelection(); }
    virtual void GetSelection(long *from, long *to) const wxOVERRIDE { wxTextEntry::GetSelection(from, to); }
    virtual wxString GetStringSelection() const wxOVERRIDE { return wxItemContainer::GetStringSelection(); }
    virtual void Clear() wxOVERRIDE;
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }
    virtual void Popup();
    virtual void Dismiss();
    virtual const wxTextEntry *WXGetTextEntry() const wxOVERRIDE { return this; }
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
protected:
    virtual wxWindow *GetEditableWindow() wxOVERRIDE { return this; }
    virtual struct olui_el *OLGetTextElement() const wxOVERRIDE;
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
public:

    wxDECLARE_DYNAMIC_CLASS(wxComboBox);
    wxDECLARE_NO_COPY_CLASS(wxComboBox);
};

#endif // _WX_OPENLAYOUT_COMBOBOX_H_
