/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dialog.h
// Purpose:     wxDialog
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DIALOG_H_
#define _WX_OPENLAYOUT_DIALOG_H_

class WXDLLIMPEXP_FWD_CORE wxGUIEventLoop;

class WXDLLIMPEXP_CORE wxDialog : public wxDialogBase
{
public:
    wxDialog() { Init(); }
    wxDialog(wxWindow *parent, wxWindowID id, const wxString& title,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxDEFAULT_DIALOG_STYLE,
             const wxString& name = wxASCII_STR(wxDialogNameStr))
    {
        Init();
        Create(parent, id, title, pos, size, style, name);
    }
    virtual ~wxDialog();

    bool Create(wxWindow *parent, wxWindowID id, const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_DIALOG_STYLE,
                const wxString& name = wxASCII_STR(wxDialogNameStr));

    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual int ShowModal() wxOVERRIDE;
    virtual void EndModal(int retCode) wxOVERRIDE;
    virtual bool IsModal() const wxOVERRIDE { return m_isModal; }

private:
    void Init() { m_isModal = false; m_modalLoop = NULL; }

    bool m_isModal;
    wxGUIEventLoop *m_modalLoop;

    wxDECLARE_DYNAMIC_CLASS(wxDialog);
    wxDECLARE_NO_COPY_CLASS(wxDialog);
};

#endif // _WX_OPENLAYOUT_DIALOG_H_
