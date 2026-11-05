/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/listbox.h
// Purpose:     wxListBox: an olui list
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_LISTBOX_H_
#define _WX_OPENLAYOUT_LISTBOX_H_

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    wxListBox() {}
    wxListBox(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr))
    {
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxListBox(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr))
    {
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr));
    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxListBoxNameStr));

    virtual bool IsSelected(int n) const wxOVERRIDE;
    virtual int GetSelections(wxArrayInt& aSelections) const wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }
    // the text of a row as olui draws it
    wxString OLRowText(unsigned int n) const;
    virtual void OLElementMade() wxOVERRIDE;

protected:
    virtual void DoSetFirstItem(int n) wxOVERRIDE;
    virtual void DoSetSelection(int n, bool select) wxOVERRIDE;
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    // the markup of item n (a check box first in a wxCheckListBox)
    virtual wxString OLItemMarkup(unsigned int n) const;
public:

    // wxItemContainer
    virtual unsigned int GetCount() const wxOVERRIDE { return (unsigned int)m_olItems.size(); }
    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;
protected:
    virtual int DoInsertItems(const wxArrayStringsAdapter& items, unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;
    virtual void DoSetItemClientData(unsigned int n, void *clientData) wxOVERRIDE;
    virtual void *DoGetItemClientData(unsigned int n) const wxOVERRIDE;
    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int pos) wxOVERRIDE;
    // rebuilds the olui element's items from m_olItems
    void OLSyncItems();

    wxArrayString m_olItems;
    wxVector<void *> m_olData;
public:

protected:
    wxVector<bool> m_olSelected;

    wxDECLARE_DYNAMIC_CLASS(wxListBox);
    wxDECLARE_NO_COPY_CLASS(wxListBox);
};

#endif // _WX_OPENLAYOUT_LISTBOX_H_
