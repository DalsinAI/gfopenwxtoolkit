/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/frame.h
// Purpose:     wxFrame: a top-level window with an Intuition menu strip
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_FRAME_H_
#define _WX_OPENLAYOUT_FRAME_H_

#include "wx/openlayout/ndktypes.h"


class WXDLLIMPEXP_CORE wxFrame : public wxFrameBase
{
public:
    wxFrame() { Init(); }
    wxFrame(wxWindow *parent, wxWindowID id, const wxString& title,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            long style = wxDEFAULT_FRAME_STYLE,
            const wxString& name = wxASCII_STR(wxFrameNameStr))
    {
        Init();
        Create(parent, id, title, pos, size, style, name);
    }
    virtual ~wxFrame();

    bool Create(wxWindow *parent, wxWindowID id, const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxDEFAULT_FRAME_STYLE,
                const wxString& name = wxASCII_STR(wxFrameNameStr));

#if wxUSE_MENUS
    virtual void SetMenuBar(wxMenuBar *menubar) wxOVERRIDE;
#endif
    virtual wxPoint GetClientAreaOrigin() const wxOVERRIDE;

    virtual wxOL_Menu *OLGetMenuStrip() const wxOVERRIDE;
    virtual void OLMenuPicked(unsigned short code) wxOVERRIDE;
    virtual void OLUpdateMenus() wxOVERRIDE;

protected:
    virtual void DoGetClientSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoSetClientSize(int width, int height) wxOVERRIDE;
#if wxUSE_STATUSBAR
    virtual void PositionStatusBar() wxOVERRIDE;
#endif
#if wxUSE_TOOLBAR
    virtual void PositionToolBar() wxOVERRIDE;
#endif

private:
    void Init() {}
    void OLOnSize(wxSizeEvent& event);
    wxDECLARE_DYNAMIC_CLASS(wxFrame);
};

#endif // _WX_OPENLAYOUT_FRAME_H_
