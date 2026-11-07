/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/window.h
// Purpose:     wxWindow: an element in its top-level window's GFOpenLayout UI
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_WINDOW_H_
#define _WX_OPENLAYOUT_WINDOW_H_

struct olui;
struct olui_el;
struct olui_event;
struct RastPort;
class WXDLLIMPEXP_FWD_CORE wxNonOwnedWindow;
class WXDLLIMPEXP_FWD_CORE wxScrollBar;
class WXDLLIMPEXP_FWD_CORE wxScrollEvent;

// Every wxWindow is a positioned element in the GFOpenLayout UI of the
// top-level window it is in; a top-level window owns that UI. A window wx
// draws itself (EVT_PAINT) holds a picture element filling it: olui calls
// back to draw it, and the window gets its wxPaintEvent there. A native
// control is an olui widget instead (OLGetMarkup), and olui runs it.
class WXDLLIMPEXP_CORE wxWindowOpenLayout : public wxWindowBase
{
public:
    wxWindowOpenLayout() { Init(); }
    wxWindowOpenLayout(wxWindow *parent, wxWindowID id,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& size = wxDefaultSize,
                       long style = 0,
                       const wxString& name = wxASCII_STR(wxPanelNameStr))
    {
        Init();
        Create(parent, id, pos, size, style, name);
    }
    virtual ~wxWindowOpenLayout();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxASCII_STR(wxPanelNameStr));

    virtual void SetLabel(const wxString& label) wxOVERRIDE { m_label = label; }
    virtual wxString GetLabel() const wxOVERRIDE { return m_label; }

    virtual void Raise() wxOVERRIDE;
    virtual void Lower() wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual void SetFocus() wxOVERRIDE;
    virtual bool Reparent(wxWindowBase *newParent) wxOVERRIDE;
    virtual void WarpPointer(int x, int y) wxOVERRIDE;
    virtual void Refresh(bool eraseBackground = true, const wxRect *rect = NULL) wxOVERRIDE;
    virtual void Update() wxOVERRIDE;
    virtual bool SetCursor(const wxCursor& cursor) wxOVERRIDE;
    virtual bool SetFont(const wxFont& font) wxOVERRIDE;
    virtual int GetCharHeight() const wxOVERRIDE;
    virtual int GetCharWidth() const wxOVERRIDE;
    virtual void SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh = true) wxOVERRIDE;
    virtual void SetScrollPos(int orient, int pos, bool refresh = true) wxOVERRIDE;
    virtual int GetScrollPos(int orient) const wxOVERRIDE;
    virtual int GetScrollThumb(int orient) const wxOVERRIDE;
    virtual int GetScrollRange(int orient) const wxOVERRIDE;
    virtual void ScrollWindow(int dx, int dy, const wxRect *rect = NULL) wxOVERRIDE;
    virtual bool IsDoubleBuffered() const wxOVERRIDE { return true; }
    virtual WXWidget GetHandle() const wxOVERRIDE { return (WXWidget)this; }
#if wxUSE_DRAG_AND_DROP
    virtual void SetDropTarget(wxDropTarget *dropTarget) wxOVERRIDE;
    virtual void DragAcceptFiles(bool accept) wxOVERRIDE;
#endif

    // ---- the port's own -------------------------------------------------
    // the top-level window this one is in (itself for one)
    wxNonOwnedWindow *OLGetTLW() const { return m_olTLW; }
    // its UI and element (NULL until it is in a top-level window)
    struct olui *OLGetUI() const;
    struct olui_el *OLGetElement() const { return m_olEl; }
    // where its children's elements go (its element; a top-level window's
    // is the UI's root, NULL)
    virtual struct olui_el *OLGetChildContainer() const { return m_olEl; }
    // the markup of its element: a native control gives its widget's (with
    // id set to OLId()); by default a picture filling it, which wx draws
    virtual wxString OLGetMarkup() const;
    // the element's id in the UI's markup
    wxString OLId() const;
    // an olui event for this window's widget (a click, a change ...)
    virtual void OLHandleEvent(const struct olui_event& ev);
    // true for a native control: olui has its mouse and keys
    virtual bool OLIsNative() const { return false; }
    // a native control with a picture in it (a bitmap button) has a picture
    // number too: olui asks wx to draw it
    virtual bool OLWantsImage() const { return false; }
    // the element was just made (set what its markup can't say)
    virtual void OLElementMade() {}
    // true for a window that draws straight into the Intuition window's
    // RastPort (wxGLCanvas): its element is a plain box olui copies its
    // buffer around, and OLDrawOwn draws it each time the window is drawn
    // where it is (x, y in the window's RastPort)
    virtual bool OLOwnDrawn() const { return false; }
    virtual void OLDrawOwn(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
    // draws it (a paint event) at x, y of rp, cut to w by h, olui having
    // asked for its picture there
    void OLPaint(struct RastPort *rp, int x, int y, int w, int h);
    // the picture number olui knows its picture by (0: none)
    int OLGetImage() const { return m_olImage; }
    // its box in its top-level window's inside (olui's pixels)
    wxRect OLGetBoxInTLW() const;
    // mouse and keys the TLW gives it (x, y in its client coordinates)
    bool OLMouse(int what, int x, int y, unsigned qualifiers);
    bool OLKey(unsigned rawkey, unsigned code, unsigned qualifiers, bool up, bool vanilla);
    // the element is made again with the new markup (a style that changed
    // the widget, a label)
    void OLRecreate();

protected:
    virtual void DoGetTextExtent(const wxString& string, int *x, int *y,
                                 int *descent = NULL, int *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const wxOVERRIDE;
    virtual void DoClientToScreen(int *x, int *y) const wxOVERRIDE;
    virtual void DoScreenToClient(int *x, int *y) const wxOVERRIDE;
    virtual void DoGetPosition(int *x, int *y) const wxOVERRIDE;
    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoGetClientSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;
    virtual void DoSetClientSize(int width, int height) wxOVERRIDE;
    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;
    virtual void DoCaptureMouse() wxOVERRIDE;
    virtual void DoReleaseMouse() wxOVERRIDE;
    virtual void DoEnable(bool enable) wxOVERRIDE;
#if wxUSE_MENUS
    virtual bool DoPopupMenu(wxMenu *menu, int x, int y) wxOVERRIDE;
#endif
#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip(wxToolTip *tip) wxOVERRIDE;
#endif

    // makes its element in its parent's (Create, Reparent); a top-level
    // window makes none
    void OLMakeElement();
    void OLDropElement();
    // after a Reparent: its children's elements made again in its own, in
    // the top-level window it is in now
    void OLRehomeChildren();
    // places and sizes the element as m_rect says
    void OLPlace();

    wxNonOwnedWindow *m_olTLW;
    struct olui_el *m_olEl;
    int m_olImage;
    // the style attribute of the element's own tag in its markup, which
    // OLPlace() puts first (olui_set_style() replaces the attribute)
    wxString m_olMarkupStyle;
    wxRect m_rect;          // in the parent's client coordinates
    wxString m_label;
    bool m_olPainting;

    struct ScrollInfo { int pos, thumb, range; } m_scroll[2];
    // the window's own scroll bars (SetScrollbar), wx's bars drawn at its
    // right and bottom edges; they are not in its children list, so sizers
    // and ScrollWindow leave them be
    wxScrollBar *m_olBar[2];
    bool OLBarShown(int i) const;
    void OLUpdateBars();
    void OLBarScrolled(wxScrollEvent& event);

private:
    void Init();

    friend class wxNonOwnedWindow;
    wxDECLARE_DYNAMIC_CLASS(wxWindowOpenLayout);
    wxDECLARE_NO_COPY_CLASS(wxWindowOpenLayout);
};

#endif // _WX_OPENLAYOUT_WINDOW_H_
