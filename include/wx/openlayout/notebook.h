/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/notebook.h
// Purpose:     wxNotebook: olui tabs over the pages
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_NOTEBOOK_H_
#define _WX_OPENLAYOUT_NOTEBOOK_H_

class WXDLLIMPEXP_CORE wxNotebook : public wxNotebookBase
{
public:
    wxNotebook() {}
    wxNotebook(wxWindow *parent, wxWindowID id,
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = 0, const wxString& name = wxASCII_STR(wxNotebookNameStr))
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxString& name = wxASCII_STR(wxNotebookNameStr));

    virtual void SetPadding(const wxSize& padding) wxOVERRIDE;
    virtual void SetTabSize(const wxSize& sz) wxOVERRIDE;
    virtual bool SetPageText(size_t n, const wxString& text) wxOVERRIDE;
    virtual wxString GetPageText(size_t n) const wxOVERRIDE;
    virtual int GetPageImage(size_t n) const wxOVERRIDE;
    virtual bool SetPageImage(size_t n, int imageId) wxOVERRIDE;
    virtual bool InsertPage(size_t n, wxWindow *page, const wxString& text,
                            bool bSelect = false, int imageId = -1) wxOVERRIDE;
    virtual wxSize CalcSizeFromPage(const wxSize& sizePage) const wxOVERRIDE;
    virtual int SetSelection(size_t nPage) wxOVERRIDE { return DoSetSelection(nPage, SetSelection_SendEvent); }
    virtual int ChangeSelection(size_t nPage) wxOVERRIDE { return DoSetSelection(nPage); }
    virtual bool DeleteAllPages() wxOVERRIDE;
    virtual void DoSetSize(int x, int y, int w, int h, int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }
    virtual void OLElementMade() wxOVERRIDE;

protected:
    virtual wxWindow *DoRemovePage(size_t page) wxOVERRIDE;
    virtual int DoSetSelection(size_t nPage, int flags = 0) wxOVERRIDE;
    // the tab strip's height
    int OLTabHeight() const;
    void OLPlacePages();

private:
    wxArrayString m_olTexts;
    wxVector<int> m_olImages;

    wxDECLARE_DYNAMIC_CLASS(wxNotebook);
    wxDECLARE_NO_COPY_CLASS(wxNotebook);
};

#endif // _WX_OPENLAYOUT_NOTEBOOK_H_
