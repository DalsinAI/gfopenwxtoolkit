/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/toolbar.h
// Purpose:     wxToolBar: a row of tool buttons wx draws, each its own
//              element (so each has its own tooltip), and the controls
//              added to it
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TOOLBAR_H_
#define _WX_OPENLAYOUT_TOOLBAR_H_

class WXDLLIMPEXP_CORE wxToolBar : public wxToolBarBase
{
public:
    wxToolBar() {}
    wxToolBar(wxWindow *parent, wxWindowID id,
              const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize,
              long style = wxTB_DEFAULT_STYLE | wxNO_BORDER,
              const wxString& name = wxASCII_STR(wxToolBarNameStr))
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxTB_DEFAULT_STYLE | wxNO_BORDER,
                const wxString& name = wxASCII_STR(wxToolBarNameStr));

    virtual wxToolBarToolBase *FindToolForPosition(wxCoord x, wxCoord y) const wxOVERRIDE;
    virtual void SetWindowStyleFlag(long style) wxOVERRIDE;
    virtual void SetToolShortHelp(int id, const wxString& helpString) wxOVERRIDE;
    virtual void SetToolNormalBitmap(int id, const wxBitmapBundle& bitmap) wxOVERRIDE;
    virtual void SetToolDisabledBitmap(int id, const wxBitmapBundle& bitmap) wxOVERRIDE;
    virtual bool Realize() wxOVERRIDE;
    virtual bool AcceptsFocus() const wxOVERRIDE { return false; }

    virtual wxToolBarToolBase *CreateTool(int toolid, const wxString& label,
                                          const wxBitmapBundle& bmpNormal,
                                          const wxBitmapBundle& bmpDisabled = wxNullBitmap,
                                          wxItemKind kind = wxITEM_NORMAL,
                                          wxObject *clientData = NULL,
                                          const wxString& shortHelp = wxEmptyString,
                                          const wxString& longHelp = wxEmptyString) wxOVERRIDE;
    virtual wxToolBarToolBase *CreateTool(wxControl *control, const wxString& label) wxOVERRIDE;

    // a tool button was clicked (the port's buttons call it)
    void OLToolClicked(wxToolBarToolBase *tool);

protected:
    virtual bool DoInsertTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;
    virtual bool DoDeleteTool(size_t pos, wxToolBarToolBase *tool) wxOVERRIDE;
    virtual void DoEnableTool(wxToolBarToolBase *tool, bool enable) wxOVERRIDE;
    virtual void DoToggleTool(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;
    virtual void DoSetToggle(wxToolBarToolBase *tool, bool toggle) wxOVERRIDE;
    virtual wxSize DoGetBestSize() const wxOVERRIDE;

private:
    // where each tool goes, and the bar's size, for Realize and best size
    wxSize OLLayout(bool place);
    void OnSize(wxSizeEvent& event);
    void OnPaint(wxPaintEvent& event);

    wxDECLARE_DYNAMIC_CLASS(wxToolBar);
    wxDECLARE_NO_COPY_CLASS(wxToolBar);
};

#endif // _WX_OPENLAYOUT_TOOLBAR_H_
