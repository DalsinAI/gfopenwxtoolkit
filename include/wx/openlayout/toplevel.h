/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/toplevel.h
// Purpose:     wxTopLevelWindow
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TOPLEVEL_H_
#define _WX_OPENLAYOUT_TOPLEVEL_H_

class WXDLLIMPEXP_CORE wxTopLevelWindowOpenLayout : public wxTopLevelWindowBase
{
public:
    wxTopLevelWindowOpenLayout() { Init(); }
    wxTopLevelWindowOpenLayout(wxWindow *parent, wxWindowID id, const wxString& title,
                               const wxPoint& pos = wxDefaultPosition,
                               const wxSize& size = wxDefaultSize,
                               long style = wxDEFAULT_FRAME_STYLE,
                               const wxString& name = wxASCII_STR(wxFrameNameStr))
    {
        Init();
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxASCII_STR(wxFrameNameStr));

    virtual void Maximize(bool maximize = true) wxOVERRIDE;
    virtual bool IsMaximized() const wxOVERRIDE;
    virtual void Iconize(bool iconize = true) wxOVERRIDE;
    virtual bool IsIconized() const wxOVERRIDE;
    virtual void Restore() wxOVERRIDE;
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) wxOVERRIDE;
    virtual bool IsFullScreen() const wxOVERRIDE { return m_fullScreen; }
    virtual void SetTitle(const wxString& title) wxOVERRIDE;
    virtual wxString GetTitle() const wxOVERRIDE { return m_title; }
    virtual void SetIcons(const wxIconBundle& icons) wxOVERRIDE { wxTopLevelWindowBase::SetIcons(icons); }
    virtual bool IsActive() wxOVERRIDE;
    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO) wxOVERRIDE { (void)flags; }

    virtual wxString OLGetTitle() const wxOVERRIDE { return m_title; }

protected:
    wxString m_title;
    bool m_fullScreen;
    bool m_maximized;
    wxRect m_restore;

private:
    void Init();
    wxDECLARE_NO_COPY_CLASS(wxTopLevelWindowOpenLayout);
};

#endif // _WX_OPENLAYOUT_TOPLEVEL_H_
