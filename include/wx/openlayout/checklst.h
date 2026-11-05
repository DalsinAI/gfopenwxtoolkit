/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/checklst.h
// Purpose:     wxCheckListBox: an olui list with a check box in each item
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CHECKLST_H_
#define _WX_OPENLAYOUT_CHECKLST_H_

class WXDLLIMPEXP_CORE wxCheckListBox : public wxCheckListBoxBase
{
public:
    wxCheckListBox() {}
    wxCheckListBox(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr))
    {
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxCheckListBox(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr))
    {
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr));
    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr));

    virtual bool IsChecked(unsigned int item) const wxOVERRIDE;
    virtual void Check(unsigned int item, bool check = true) wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
protected:
    virtual wxString OLItemMarkup(unsigned int n) const wxOVERRIDE;
    virtual int DoInsertItems(const wxArrayStringsAdapter& items, unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;
    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int pos) wxOVERRIDE;
private:
    wxVector<bool> m_olChecked;
public:

    wxDECLARE_DYNAMIC_CLASS(wxCheckListBox);
    wxDECLARE_NO_COPY_CLASS(wxCheckListBox);
};

#endif // _WX_OPENLAYOUT_CHECKLST_H_
