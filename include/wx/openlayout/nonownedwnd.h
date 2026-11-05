/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/nonownedwnd.h
// Purpose:     wxNonOwnedWindow: an Intuition window holding a GFOpenLayout UI
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_NONOWNEDWND_H_
#define _WX_OPENLAYOUT_NONOWNEDWND_H_

#include "wx/openlayout/ndktypes.h"

struct olui;
struct olui_amiga;
struct IntuiMessage;

class WXDLLIMPEXP_CORE wxNonOwnedWindow : public wxNonOwnedWindowBase
{
public:
    wxNonOwnedWindow() { Init(); }
    virtual ~wxNonOwnedWindow();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxASCII_STR(wxPanelNameStr));

    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual void Raise() wxOVERRIDE;
    virtual void Lower() wxOVERRIDE;
    virtual void Update() wxOVERRIDE;
    virtual void SetFocus() wxOVERRIDE;
    virtual void WarpPointer(int x, int y) wxOVERRIDE;
    virtual bool SetCursor(const wxCursor& cursor) wxOVERRIDE;

    // ---- the port's own -------------------------------------------------
    wxOL_Window *OLGetWindow() const { return m_olWin; }
    struct olui *OLGetOwnUI() const { return m_olUI; }
    virtual struct olui_el *OLGetChildContainer() const wxOVERRIDE { return NULL; }
    // one message from its Intuition window, before it is replied
    void OLHandleMessage(struct IntuiMessage *msg);
    // draws what changed (the event loop, when the queue is empty)
    void OLDrawPending();
    // the window's Intuition title and its flags, set before it opens
    virtual wxString OLGetTitle() const { return wxString(); }
    virtual unsigned long OLGetFlags() const;
    // the menu strip it shows (wxFrame's menu bar), NULL: none
    virtual wxOL_Menu *OLGetMenuStrip() const { return NULL; }
    virtual void OLMenuPicked(unsigned short code) { (void)code; }
    // the menus changed since the strip was made: it is made again
    virtual void OLUpdateMenus() {}
    // the windows with the mouse captured, the focus, and the hover
    wxWindow *OLGetFocus() const { return m_olFocus; }
    void OLSetFocus(wxWindow *win);
    // a window's element went or changed: forget it in the tables
    void OLForget(wxWindow *win);
    // a window drawn by wx asks to be drawn again, all of it or rect of it
    // (its client coordinates)
    void OLInvalidate(wxWindow *win, const wxRect *rect);
    // relayout and draw after elements were placed
    void OLNeedLayout() { m_olLayoutNeeded = true; }
    // picture numbers for the windows wx draws
    int OLAllocImage(wxWindow *win);
    void OLFreeImage(int image);
    wxWindow *OLWindowForImage(int image) const;

protected:
    virtual void DoGetPosition(int *x, int *y) const wxOVERRIDE;
    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoGetClientSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;
    virtual void DoSetClientSize(int width, int height) wxOVERRIDE;
    virtual void DoClientToScreen(int *x, int *y) const wxOVERRIDE;
    virtual void DoScreenToClient(int *x, int *y) const wxOVERRIDE;
    virtual void DoCaptureMouse() wxOVERRIDE;
    virtual void DoReleaseMouse() wxOVERRIDE;

    // opens the Intuition window (Show), closes it (destructor, Hide)
    bool OLOpen();
    void OLClose();
    // the window is told its new size (NEWSIZE)
    void OLSized();

    wxOL_Window *m_olWin;
    struct olui *m_olUI;
    struct olui_amiga *m_olAmiga;
    wxWindow *m_olFocus;
    wxWindow *m_olHover;
    bool m_olLayoutNeeded;
    bool m_olBusy;          // handling one of its messages: closing waits
    bool m_olCloseLater;
    bool m_olActive;
    wxVector<wxWindow*> m_olImages;   // index: picture number - 1
    wxWindow *m_olPressed;  // a button went down in it (drawn by wx): it has the mouse until it goes up
    unsigned long m_olDownSecs, m_olDownMicros;
    int m_olDownX, m_olDownY, m_olDownButton;
    void *m_olQueue;        // olui's events, given to wx after olui returns
    bool m_olRmbTrap;
    // the window's title in Latin-1: Intuition keeps the pointer, so it
    // lives (AllocVec) until the window closes or the title changes
    char *m_olTitleText;

public:
    // olui's handler: the event, to be handled after olui returns
    void OLQueue(const struct olui_event *ev);
    // the events of that type olui queued for win: dropped (a program's own
    // change, which wx does not report back)
    void OLDropQueued(wxWindow *win, int type);
    // the right button is the window's (a context menu), not the menus'
    void OLSetRmbTrap(bool on);

private:
    void Init();
    void OLRunQueue();
    void OLMouseMessage(struct IntuiMessage *msg);
    void OLKeyMessage(struct IntuiMessage *msg);
    wxWindow *OLWindowAt(int x, int y, int *cx, int *cy) const;

    wxDECLARE_NO_COPY_CLASS(wxNonOwnedWindow);
};

#endif // _WX_OPENLAYOUT_NONOWNEDWND_H_
