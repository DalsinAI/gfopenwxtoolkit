/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/choice.h
// Purpose:     wxChoice: an olui select
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CHOICE_H_
#define _WX_OPENLAYOUT_CHOICE_H_

class WXDLLIMPEXP_CORE wxChoice : public wxChoiceBase
{
public:
    wxChoice() : m_olSel(wxNOT_FOUND) {}
    wxChoice(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxChoiceNameStr)) : m_olSel(wxNOT_FOUND)
    {
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxChoice(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxChoiceNameStr)) : m_olSel(wxNOT_FOUND)
    {
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxChoiceNameStr));
    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, const wxArrayString& choices, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxChoiceNameStr));

    virtual void SetSelection(int n) wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE { return m_olSel; }

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }
    virtual void OLElementMade() wxOVERRIDE;

    virtual wxSize DoGetBestSize() const wxOVERRIDE;

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
    int m_olSel;

    wxDECLARE_DYNAMIC_CLASS(wxChoice);
    wxDECLARE_NO_COPY_CLASS(wxChoice);
};

#endif // _WX_OPENLAYOUT_CHOICE_H_
