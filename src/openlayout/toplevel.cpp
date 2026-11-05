/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/toplevel.cpp
// Purpose:     wxNonOwnedWindow, wxTopLevelWindow, wxFrame, wxDialog and
//              wxPopupWindow: an Intuition window on the public screen,
//              holding one GFOpenLayout UI, whose elements are wx's windows
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/frame.h"
    #include "wx/dialog.h"
    #include "wx/button.h"
    #include "wx/menu.h"
    #include "wx/statusbr.h"
    #include "wx/toolbar.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/popupwin.h"
#include "wx/stattext.h"
#include "wx/statbox.h"
#include "wx/statline.h"
#include "wx/statbmp.h"
#include "wx/checkbox.h"
#include "wx/radiobut.h"
#include "wx/evtloop.h"
#include "wx/modalhook.h"

#include <deque>

#include <exec/ports.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include "private.h"

// keymap.library's MapRawKey: the character of a key, as the keymap has it
static struct Library *gs_keymapBase = NULL;
#define wxOL_MAPRAWKEY(ie, buf, len, km) LP4(0x2a, WORD, MapRawKey, struct InputEvent *, (ie), a0, STRPTR, (buf), a1, \
    LONG, (len), d1, struct KeyMap *, (km), a2, , gs_keymapBase)

#ifndef IDCMP_EXTENDEDMOUSE
    #define IDCMP_EXTENDEDMOUSE 0x08000000UL
#endif
// IDCMP_EXTENDEDMOUSE's Code and IAddress (intuition/intuition.h V47)
#define wxOL_INTUIWHEELDATA (1 << 15)
struct wxOLWheelData { UWORD Version, Reserved; WORD WheelX, WheelY; };
#define wxOL_RAW_WHEEL_UP 0x7a
#define wxOL_RAW_WHEEL_DOWN 0x7b

// ---- the windows open, and the UI the others share the look with -------------------

static wxVector<wxNonOwnedWindow *> gs_open;
static struct olui *gs_masterUI = NULL;
static bool gs_themed = false;

// what every window's UI starts with: the root as wx places it
static const char gs_baseCSS[] = "window{padding:0;gap:0;display:block}";

static struct olui *MasterUI()
{
    if ( !gs_masterUI )
    {
        gs_masterUI = olui_new();
        char err[96];
        if ( gs_masterUI )
            olui_load(gs_masterUI, "<div style='display:none'></div>", gs_baseCSS, err, sizeof err);
    }
    return gs_masterUI;
}

void wxOLFreeMasterUI()
{
    if ( gs_masterUI )
        olui_free(gs_masterUI);
    gs_masterUI = NULL;
    gs_themed = false;
}

// every open window, for the event loop
size_t wxOLOpenWindowCount() { return gs_open.size(); }
wxNonOwnedWindow *wxOLOpenWindow(size_t i) { return i < gs_open.size() ? gs_open[i] : NULL; }

// ---- olui's events, queued ----------------------------------------------------------

namespace
{
struct QueuedEvent
{
    wxWindow *win;
    olui_event ev;
    std::string id, value;
};
typedef std::deque<QueuedEvent> EventQueue;
}

#define M_QUEUE (*(EventQueue *)m_olQueue)

static void OLUIHandler(olui *WXUNUSED(ui), const olui_event *ev, void *userdata)
{
    ((wxNonOwnedWindow *)userdata)->OLQueue(ev);
}

// the picture olui asks for: the wx window it is drawn by
static void OLUIImage(void *userdata, struct RastPort *rp, const olui_el *WXUNUSED(el), int image,
                      int x, int y, int w, int h)
{
    wxWindow *win = ((wxNonOwnedWindow *)userdata)->OLWindowForImage(image);
    if ( win )
        win->OLPaint(rp, x, y, w, h);
}

// ---- wxNonOwnedWindow ---------------------------------------------------------------

void wxNonOwnedWindow::Init()
{
    m_olWin = NULL;
    m_olUI = NULL;
    m_olAmiga = NULL;
    m_olFocus = NULL;
    m_olHover = NULL;
    m_olLayoutNeeded = false;
    m_olBusy = false;
    m_olCloseLater = false;
    m_olActive = false;
    m_olPressed = NULL;
    m_olDownSecs = m_olDownMicros = 0;
    m_olDownX = m_olDownY = 0;
    m_olDownButton = 0;
    m_olQueue = new EventQueue;
    m_olRmbTrap = false;
    m_olTitleText = NULL;
    m_olBoxesAsked = 0;
    m_olBoxSecs = 0;
}

bool wxNonOwnedWindow::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                              long style, const wxString& name)
{
    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    if ( parent )
        parent->AddChild(this);
    m_olTLW = this;

    struct Screen *scr = wxOLGetScreen();
    const int sw = scr ? scr->Width : 640, sh = scr ? scr->Height : 480;
    int w = size.x > 0 ? size.x : wxMin(400, sw);
    int h = size.y > 0 ? size.y : wxMin(250, sh);
    int x = pos.x != wxDefaultCoord ? pos.x : (sw - w) / 2;
    int y = pos.y != wxDefaultCoord ? pos.y : (sh - h) / 2;
    m_rect = wxRect(wxMax(x, 0), wxMax(y, 0), w, h);

    m_olUI = olui_new_sharing(MasterUI());
    if ( !m_olUI )
        return false;
    char err[96];
    if ( !olui_load(m_olUI, "<div style='display:none'></div>", NULL, err, sizeof err) )
    {
        wxLogDebug("olui_load: %s", err);
        return false;
    }
    olui_on(m_olUI, NULL, 0, OLUIHandler, this);
    return true;
}

wxNonOwnedWindow::~wxNonOwnedWindow()
{
    SendDestroyEvent();
    // the children's elements go before the UI they are in
    DestroyChildren();
    m_olBusy = false;
    OLClose();
    if ( m_olUI )
        olui_free(m_olUI);
    m_olUI = NULL;
    delete (EventQueue *)m_olQueue;
    m_olQueue = NULL;
}

unsigned long wxNonOwnedWindow::OLGetFlags() const
{
    const long style = GetWindowStyleFlag();
    unsigned long f = WFLG_REPORTMOUSE | WFLG_SIMPLE_REFRESH;
    if ( style & (wxCAPTION | wxSYSTEM_MENU) )
        f |= WFLG_DRAGBAR | WFLG_DEPTHGADGET;
    if ( style & wxCLOSE_BOX )
        f |= WFLG_CLOSEGADGET;
    if ( style & wxRESIZE_BORDER )
        f |= WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM;
    if ( !(style & (wxCAPTION | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCLOSE_BOX)) )
        f |= WFLG_BORDERLESS;
    return f;
}

// the borders a window of these flags gets on the screen (before it opens)
static void GuessBorders(struct Screen *scr, unsigned long flags, int *l, int *t, int *r, int *b)
{
    if ( !scr || (flags & WFLG_BORDERLESS) )
    {
        *l = *t = *r = *b = 0;
        return;
    }
    *l = scr->WBorLeft;
    *r = scr->WBorRight;
    *t = scr->WBorTop + ((flags & (WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_DEPTHGADGET)) ? scr->Font->ta_YSize + 1 : 0);
    *b = scr->WBorBottom;
    if ( flags & WFLG_SIZEGADGET )
    {
        *r = wxMax(*r, 18);
        *b = wxMax(*b, 10);
    }
}

static void Borders(const wxNonOwnedWindow *w, int *l, int *t, int *r, int *b)
{
    struct Window *win = w->OLGetWindow();
    if ( win )
    {
        *l = win->BorderLeft;
        *t = win->BorderTop;
        *r = win->BorderRight;
        *b = win->BorderBottom;
    }
    else
        GuessBorders(wxOLGetScreen(), w->OLGetFlags(), l, t, r, b);
}

bool wxNonOwnedWindow::OLOpen()
{
    if ( m_olWin )
        return true;
    struct Screen *scr = wxOLGetScreen();
    if ( !scr )
        return false;
    // not made active while a window has the mouse captured (a toolbar
    // dragged in a frame of its own): the active window gets the mouse
    const unsigned long flags = OLGetFlags() |
        (IsKindOf(wxCLASSINFO(wxPopupWindow)) || wxWindow::GetCapture() ? 0 : WFLG_ACTIVATE);
    int l, t, r, b;
    GuessBorders(scr, flags, &l, &t, &r, &b);
    const int iw = wxMax(m_rect.width - l - r, 1), ih = wxMax(m_rect.height - t - b, 1);
    const wxSize mn = GetMinSize();
    // Intuition keeps the title's pointer: a copy that lives with the window
    const std::string title = wxOLToLatin1(OLGetTitle());
    if ( m_olTitleText )
        FreeVec(m_olTitleText);
    m_olTitleText = NULL;
    if ( !title.empty() && !(flags & WFLG_BORDERLESS) )
    {
        m_olTitleText = (char *)AllocVec(title.size() + 1, MEMF_ANY);
        if ( m_olTitleText )
            memcpy(m_olTitleText, title.c_str(), title.size() + 1);
    }

    struct TagItem tags[] = {
        { WA_Left, (ULONG)m_rect.x },
        { WA_Top, (ULONG)m_rect.y },
        { WA_InnerWidth, (ULONG)iw },
        { WA_InnerHeight, (ULONG)ih },
        { WA_MinWidth, (ULONG)(mn.x > 0 ? mn.x : 60) },
        { WA_MinHeight, (ULONG)(mn.y > 0 ? mn.y : 30) },
        { WA_MaxWidth, (ULONG)~0UL },
        { WA_MaxHeight, (ULONG)~0UL },
        { WA_Flags, flags },
        { WA_IDCMP, 0 },
        { WA_PubScreen, (ULONG)scr },
        { WA_AutoAdjust, TRUE },
        { WA_NewLookMenus, TRUE },
        { WA_Title, (ULONG)m_olTitleText },
        { TAG_END, 0 }
    };
    m_olWin = OpenWindowTagList(NULL, tags);
    if ( !m_olWin )
    {
        wxLogError("Could not open the window \"%s\".", OLGetTitle());
        return false;
    }
    m_olWin->UserData = (BYTE *)this;
    m_olWin->UserPort = wxOLSharedPort();
    ModifyIDCMP(m_olWin, (olui_amiga_idcmp() & ~IDCMP_VANILLAKEY) | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW |
                         IDCMP_MENUPICK | IDCMP_CHANGEWINDOW | IDCMP_EXTENDEDMOUSE);

    m_olAmiga = olui_amiga_new(m_olUI, m_olWin);
    if ( !gs_themed )
    {
        // the look is loaded once: every window's UI shares the sheet
        gs_themed = olui_amiga_theme(m_olAmiga, NULL) != 0;
        // text in the screen's font at its size, as wx measures it
        // (wxSYS_DEFAULT_GUI_FONT): olui's own is 13 pixels
        if ( struct TextFont *tf = wxOLScreenFont() )
        {
            char css[48];
            snprintf(css, sizeof css, "window{font-size:%dpx}", (int)tf->tf_YSize);
            olui_add_css(MasterUI(), css);
        }
    }
    olui_amiga_keys(m_olAmiga, OLUI_AMIGA_KEYS_FOCUS);
    olui_amiga_images(m_olAmiga, OLUIImage, this);
    if ( struct Menu *strip = OLGetMenuStrip() )
        SetMenuStrip(m_olWin, strip);
    m_olRmbTrap = false;
    OLSetRmbTrap(false);
    // opened active, its IDCMP_ACTIVEWINDOW came before it had IDCMP
    m_olActive = IntuitionBase->ActiveWindow == m_olWin;
    gs_open.push_back(this);

    m_olBoxesAsked = 0;
    OLSized();
    m_olLayoutNeeded = true;
    OLDrawPending();
    return true;
}

void wxNonOwnedWindow::OLClose()
{
    if ( !m_olWin )
        return;
    if ( m_olBusy )
    {
        // olui is in the middle of one of its messages
        m_olCloseLater = true;
        return;
    }
    struct Window *win = m_olWin;
    // the window opened active has had no IDCMP_ACTIVEWINDOW (it had no
    // IDCMP then): Intuition says whether it is the active one
    const bool wasActive = m_olActive || IntuitionBase->ActiveWindow == win;
    if ( win->MenuStrip )
        ClearMenuStrip(win);
    olui_amiga_free(m_olAmiga);
    m_olAmiga = NULL;

    // the shared port: this window's messages go back, then it has none
    Forbid();
    struct MsgPort *port = win->UserPort;
    if ( port )
    {
        struct Node *n = port->mp_MsgList.lh_Head, *next;
        for ( ; (next = n->ln_Succ) != NULL; n = next )
        {
            struct IntuiMessage *m = (struct IntuiMessage *)n;
            if ( m->IDCMPWindow == win )
            {
                Remove(n);
                ReplyMsg(&m->ExecMessage);
            }
        }
    }
    win->UserPort = NULL;
    ModifyIDCMP(win, 0);
    Permit();
    CloseWindow(win);
    if ( m_olTitleText )
        FreeVec(m_olTitleText);
    m_olTitleText = NULL;
    m_olWin = NULL;
    m_olPressed = NULL;
    m_olHover = NULL;
    m_olActive = false;
    for ( size_t i = 0; i < gs_open.size(); i++ )
        if ( gs_open[i] == this )
        {
            gs_open.erase(gs_open.begin() + i);
            break;
        }

    // Intuition makes no window active when the active one closes: the
    // one it belongs to (a dialog's frame) is made active again, or else
    // the last of the program's windows opened
    if ( wasActive && !gs_open.empty() )
    {
        wxNonOwnedWindow *next = NULL;
        for ( wxWindow *p = GetParent(); p && !next; p = p->GetParent() )
            if ( p->IsTopLevel() )
            {
                wxNonOwnedWindow *tlw = static_cast<wxNonOwnedWindow *>(p);
                if ( tlw->m_olWin )
                    next = tlw;
            }
        if ( !next )
            next = gs_open.back();
        ActivateWindow(next->m_olWin);
    }
}

bool wxNonOwnedWindow::Show(bool show)
{
    if ( !wxWindowBase::Show(show) )
        return false;
    if ( show )
    {
        // the size wx gave it before it showed (sizers) is laid out now
        wxSizeEvent ev(GetSize(), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
        return OLOpen();
    }
    OLClose();
    return true;
}

void wxNonOwnedWindow::Raise()
{
    if ( m_olWin )
    {
        WindowToFront(m_olWin);
        ActivateWindow(m_olWin);
    }
}

void wxNonOwnedWindow::Lower()
{
    if ( m_olWin )
        WindowToBack(m_olWin);
}

void wxNonOwnedWindow::Update()
{
    OLDrawPending();
}

void wxNonOwnedWindow::SetFocus()
{
    Raise();
}

void wxNonOwnedWindow::WarpPointer(int WXUNUSED(x), int WXUNUSED(y))
{
}

bool wxNonOwnedWindow::SetCursor(const wxCursor& cursor)
{
    return wxWindowBase::SetCursor(cursor);
}

// ---- place and size ------------------------------------------------------------------

void wxNonOwnedWindow::DoGetPosition(int *x, int *y) const
{
    if ( x ) *x = m_olWin ? m_olWin->LeftEdge : m_rect.x;
    if ( y ) *y = m_olWin ? m_olWin->TopEdge : m_rect.y;
}

void wxNonOwnedWindow::DoGetSize(int *width, int *height) const
{
    if ( width ) *width = m_rect.width;
    if ( height ) *height = m_rect.height;
}

void wxNonOwnedWindow::DoGetClientSize(int *width, int *height) const
{
    int l, t, r, b;
    Borders(this, &l, &t, &r, &b);
    if ( width ) *width = wxMax(m_rect.width - l - r, 0);
    if ( height ) *height = wxMax(m_rect.height - t - b, 0);
}

void wxNonOwnedWindow::DoMoveWindow(int x, int y, int width, int height)
{
    if ( width < 1 ) width = 1;
    if ( height < 1 ) height = 1;
    const bool sized = width != m_rect.width || height != m_rect.height;
    const bool moved = x != m_rect.x || y != m_rect.y;
    m_rect = wxRect(x, y, width, height);
    if ( m_olWin && (sized || moved) )
    {
        ChangeWindowBox(m_olWin, x, y, width, height);   // CHANGEWINDOW follows
        m_olBoxesAsked++;
        m_olBoxAsked = m_rect;
        ULONG secs, micros;
        CurrentTime(&secs, &micros);
        m_olBoxSecs = secs;
    }
    if ( sized )
    {
        wxSizeEvent ev(wxSize(width, height), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
        m_olLayoutNeeded = true;
    }
    if ( moved )
    {
        wxMoveEvent ev(wxPoint(x, y), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
}

void wxNonOwnedWindow::DoSetClientSize(int width, int height)
{
    int l, t, r, b;
    Borders(this, &l, &t, &r, &b);
    DoMoveWindow(m_rect.x, m_rect.y, width + l + r, height + t + b);
}

void wxNonOwnedWindow::DoClientToScreen(int *x, int *y) const
{
    int px, py, l, t, r, b;
    DoGetPosition(&px, &py);
    Borders(this, &l, &t, &r, &b);
    const wxPoint o = GetClientAreaOrigin();
    if ( x ) *x += px + l + o.x;
    if ( y ) *y += py + t + o.y;
}

void wxNonOwnedWindow::DoScreenToClient(int *x, int *y) const
{
    int ox = 0, oy = 0;
    DoClientToScreen(&ox, &oy);
    if ( x ) *x -= ox;
    if ( y ) *y -= oy;
}

void wxNonOwnedWindow::DoCaptureMouse()
{
    wxWindowOpenLayout::DoCaptureMouse();
}

void wxNonOwnedWindow::DoReleaseMouse()
{
    wxWindowOpenLayout::DoReleaseMouse();
}

void wxNonOwnedWindow::OLSized()
{
    if ( !m_olWin )
        return;
    const wxRect now(m_olWin->LeftEdge, m_olWin->TopEdge, m_olWin->Width, m_olWin->Height);
    const bool resized = now.GetSize() != m_olSeenSize;
    m_olSeenSize = now.GetSize();
    if ( m_olBoxesAsked > 0 )
    {
        // a box the program asked for: wx has it already (DoMoveWindow).
        // Until the last one is done the reports are older boxes, which
        // would move the window back (a drag that stutters); two seconds
        // without the last one: Intuition changed the box, take what it is
        ULONG secs, micros;
        CurrentTime(&secs, &micros);
        if ( now != m_olBoxAsked && secs - m_olBoxSecs < 2 )
        {
            if ( resized )
                m_olLayoutNeeded = true;
            return;
        }
        m_olBoxesAsked = 0;
    }
    const bool sized = now.GetSize() != m_rect.GetSize();
    const bool moved = now.GetPosition() != m_rect.GetPosition();
    m_rect = now;
    if ( sized )
    {
        wxSizeEvent ev(now.GetSize(), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
    if ( moved )
    {
        wxMoveEvent ev(now.GetPosition(), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
    // a move alone needs no new layout: what it uncovered comes as REFRESHWINDOW
    if ( sized || resized )
        m_olLayoutNeeded = true;
}

// ---- drawing --------------------------------------------------------------------------

void wxNonOwnedWindow::OLInvalidate(wxWindow *win, const wxRect *rect)
{
    // olui has the element marked (Refresh): it is drawn by OLDrawPending
    (void)win;
    (void)rect;
}

void wxNonOwnedWindow::OLDrawPending()
{
    if ( !m_olAmiga || !m_olWin )
        return;
    OLUpdateMenus();
    if ( m_olLayoutNeeded )
    {
        m_olLayoutNeeded = false;
        olui_amiga_layout(m_olAmiga);
    }
    else
        olui_amiga_draw(m_olAmiga);
}

int wxNonOwnedWindow::OLAllocImage(wxWindow *win)
{
    for ( size_t i = 0; i < m_olImages.size(); i++ )
        if ( !m_olImages[i] )
        {
            m_olImages[i] = win;
            return (int)i + 1;
        }
    m_olImages.push_back(win);
    return (int)m_olImages.size();
}

void wxNonOwnedWindow::OLFreeImage(int image)
{
    if ( image > 0 && (size_t)image <= m_olImages.size() )
        m_olImages[image - 1] = NULL;
}

wxWindow *wxNonOwnedWindow::OLWindowForImage(int image) const
{
    if ( image > 0 && (size_t)image <= m_olImages.size() )
        return m_olImages[image - 1];
    return NULL;
}

void wxNonOwnedWindow::OLForget(wxWindow *win)
{
    if ( m_olFocus == win )
        m_olFocus = NULL;
    if ( m_olHover == win )
        m_olHover = NULL;
    if ( m_olPressed == win )
        m_olPressed = NULL;
    for ( size_t i = 0; i < m_olImages.size(); i++ )
        if ( m_olImages[i] == win )
            m_olImages[i] = NULL;
    if ( m_olQueue )
        for ( EventQueue::iterator it = M_QUEUE.begin(); it != M_QUEUE.end(); ++it )
            if ( it->win == win )
                it->win = NULL;
}

// ---- focus ------------------------------------------------------------------------------

void wxNonOwnedWindow::OLSetFocus(wxWindow *win)
{
    if ( win == (wxWindow *)this )
        win = NULL;
    if ( win && win->OLIsNative() && m_olUI && win->OLGetElement() )
        olui_focus(m_olUI, win->OLGetElement());
    else if ( m_olUI && olui_focused(m_olUI) )
        olui_focus(m_olUI, NULL);
    if ( win == m_olFocus )
        return;
    wxWindow *old = m_olFocus;
    m_olFocus = win;
    if ( old )
    {
        wxFocusEvent kill(wxEVT_KILL_FOCUS, old->GetId());
        kill.SetEventObject(old);
        kill.SetWindow(win);
        old->HandleWindowEvent(kill);
    }
    if ( win )
    {
        wxFocusEvent set(wxEVT_SET_FOCUS, win->GetId());
        set.SetEventObject(win);
        set.SetWindow(old);
        win->HandleWindowEvent(set);
    }
    if ( m_olAmiga )
        olui_amiga_draw(m_olAmiga);
}

// ---- olui's events ---------------------------------------------------------------------

// menu.cpp: a pop-up menu's choice (true: it was one)
bool wxOLPopupEvent(wxNonOwnedWindow *tlw, const struct olui_event& ev);

void wxNonOwnedWindow::OLDropQueued(wxWindow *win, int type)
{
    if ( !m_olQueue )
        return;
    for ( EventQueue::iterator it = M_QUEUE.begin(); it != M_QUEUE.end(); )
    {
        if ( it->win == win && it->ev.type == type )
            it = M_QUEUE.erase(it);
        else
            ++it;
    }
}

void wxNonOwnedWindow::OLQueue(const struct olui_event *ev)
{
    // the wx window whose element it is: the element, or the one it is in
    wxWindow *win = NULL;
    for ( olui_el *el = ev->el; el && !win; el = olui_parent(el) )
        win = (wxWindow *)olui_user(el);
    if ( !win )
        win = (wxWindow *)this;     // a pop-up menu's: wxOLPopupEvent has it
    if ( !m_olQueue )
        return;
    QueuedEvent q;
    q.win = win;
    q.ev = *ev;
    q.id = ev->id ? ev->id : "";
    q.value = ev->value ? ev->value : "";
    M_QUEUE.push_back(q);
    QueuedEvent& b = M_QUEUE.back();
    b.ev.id = b.id.c_str();
    b.ev.value = ev->value ? b.value.c_str() : NULL;
}

void wxNonOwnedWindow::OLRunQueue()
{
    while ( m_olQueue && !M_QUEUE.empty() )
    {
        QueuedEvent q = M_QUEUE.front();
        M_QUEUE.pop_front();
        q.ev.id = q.id.c_str();
        q.ev.value = q.ev.value ? q.value.c_str() : NULL;
        if ( !q.win || wxOLPopupEvent(this, q.ev) )
            continue;
        if ( q.ev.type == OLUI_FOCUS )
        {
            // olui gave one of its widgets the keys (a click, Tab)
            if ( q.win != m_olFocus )
            {
                wxWindow *old = m_olFocus;
                m_olFocus = q.win;
                if ( old )
                {
                    wxFocusEvent kill(wxEVT_KILL_FOCUS, old->GetId());
                    kill.SetEventObject(old);
                    kill.SetWindow(q.win);
                    old->HandleWindowEvent(kill);
                }
                if ( m_olFocus )
                {
                    wxFocusEvent set(wxEVT_SET_FOCUS, q.win->GetId());
                    set.SetEventObject(q.win);
                    set.SetWindow(old);
                    q.win->HandleWindowEvent(set);
                }
            }
            continue;
        }
        q.win->OLHandleEvent(q.ev);
    }
}

// ---- Intuition's messages ----------------------------------------------------------------

void wxNonOwnedWindow::OLSetRmbTrap(bool on)
{
    // a window without a menu strip has the right button always
    if ( !m_olWin )
        return;
    on = on || !m_olWin->MenuStrip;
    if ( on == m_olRmbTrap )
        return;
    m_olRmbTrap = on;
    Forbid();
    if ( on )
        m_olWin->Flags |= WFLG_RMBTRAP;
    else
        m_olWin->Flags &= ~WFLG_RMBTRAP;
    Permit();
}

wxWindow *wxNonOwnedWindow::OLWindowAt(int x, int y, int *cx, int *cy) const
{
    wxWindow *win = NULL;
    for ( olui_el *el = m_olUI ? olui_hit(m_olUI, x, y) : NULL; el && !win; el = olui_parent(el) )
        win = (wxWindow *)olui_user(el);
    if ( win && win->OLIsNative() )
        return NULL;
    if ( !win )
        win = (wxWindow *)this;
    const wxRect box = win->OLGetBoxInTLW();
    if ( cx ) *cx = x - box.x;
    if ( cy ) *cy = y - box.y;
    return win;
}

// a wheel turned over x, y: the window wx draws under it, else the nearest
// one above it that takes it (a scrolled panel under its labels and boxes,
// as on other systems); false: olui's, a control that scrolls itself (a
// list, a text, a select)
static bool OLWheelToWx(wxWindow *tlw, struct olui *ui, int what, int x, int y, unsigned qual)
{
    wxWindow *win = NULL;
    for ( olui_el *el = ui ? olui_hit(ui, x, y) : NULL; el && !win; el = olui_parent(el) )
        win = (wxWindow *)olui_user(el);
    if ( win && win->OLIsNative() )
    {
        if ( !wxDynamicCast(win, wxStaticText) && !wxDynamicCast(win, wxStaticBox) &&
             !wxDynamicCast(win, wxStaticLine) && !wxDynamicCast(win, wxStaticBitmap) &&
             !wxDynamicCast(win, wxCheckBox) && !wxDynamicCast(win, wxRadioButton) &&
             !wxDynamicCast(win, wxAnyButton) )
            return false;
        win = win->GetParent();
    }
    if ( !win )
        win = tlw;
    for ( ; win; win = win->GetParent() )
    {
        if ( !win->OLIsNative() )
        {
            const wxRect box = win->OLGetBoxInTLW();
            if ( win->OLMouse(what, x - box.x, y - box.y, qual) )
                break;
        }
        if ( win->IsTopLevel() )
            break;
    }
    return true;
}

void wxNonOwnedWindow::OLMouseMessage(struct IntuiMessage *msg)
{
    const int x = msg->MouseX - m_olWin->BorderLeft, y = msg->MouseY - m_olWin->BorderTop;
    unsigned qual = msg->Qualifier;
    wxOLLastQualifier = msg->Qualifier;

    int what = wxEVT_MOTION, button = 0;
    bool down = false, up = false;
    if ( msg->Class == IDCMP_MOUSEBUTTONS )
    {
        switch ( msg->Code )
        {
            case SELECTDOWN: what = wxEVT_LEFT_DOWN; button = 1; down = true; break;
            case SELECTUP: what = wxEVT_LEFT_UP; button = 1; up = true; break;
            case MENUDOWN: what = wxEVT_RIGHT_DOWN; button = 2; down = true; break;
            case MENUUP: what = wxEVT_RIGHT_UP; button = 2; up = true; break;
            case MIDDLEDOWN: what = wxEVT_MIDDLE_DOWN; button = 3; down = true; break;
            case MIDDLEUP: what = wxEVT_MIDDLE_UP; button = 3; up = true; break;
            default: return;
        }
    }
    else if ( msg->Class == IDCMP_RAWKEY )
    {
        what = wxEVT_MOUSEWHEEL;
        if ( msg->Code == wxOL_RAW_WHEEL_DOWN )
            qual |= 0x10000;
    }

    // who has it: the window with the mouse captured, the one a button went
    // down in, else the one under it (NULL: one of olui's widgets)
    int cx = 0, cy = 0;
    wxWindow *target = NULL;
    wxWindow *cap = wxWindow::GetCapture();
    if ( cap && (msg->Class == IDCMP_MOUSEBUTTONS || msg->Class == IDCMP_MOUSEMOVE) )
    {
        // the mouse is captured in another of the program's windows: it is
        // that window's, where it is on the screen
        wxNonOwnedWindow *other = cap->OLGetTLW();
        if ( other && other != this && other->m_olWin )
        {
            struct IntuiMessage m = *msg;
            m.MouseX = msg->MouseX + m_olWin->LeftEdge - other->m_olWin->LeftEdge;
            m.MouseY = msg->MouseY + m_olWin->TopEdge - other->m_olWin->TopEdge;
            m.IDCMPWindow = other->m_olWin;
            other->OLMouseMessage(&m);
            return;
        }
    }
    if ( m_olUI && olui_popup(m_olUI) )
    {
        // a select's list or a menu is open: olui has the pointer everywhere
        olui_amiga_event(m_olAmiga, msg);
        return;
    }
    if ( what == wxEVT_MOUSEWHEEL && !(cap && cap->OLGetTLW() == this) &&
         OLWheelToWx((wxWindow *)this, m_olUI, what, x, y, qual) )
        return;
    if ( cap && cap->OLGetTLW() == this && !cap->OLIsNative() )
        target = cap;
    else if ( m_olPressed && what != wxEVT_MOUSEWHEEL )
        target = m_olPressed;
    if ( target )
    {
        const wxRect box = target->OLGetBoxInTLW();
        cx = x - box.x;
        cy = y - box.y;
    }
    else
        target = OLWindowAt(x, y, &cx, &cy);

    if ( !target )
    {
        // olui's: the hover leaves the window wx drew
        if ( m_olHover )
        {
            wxWindow *old = m_olHover;
            m_olHover = NULL;
            old->OLMouse(wxEVT_LEAVE_WINDOW, -1, -1, qual);
        }
        OLSetRmbTrap(false);
        olui_amiga_event(m_olAmiga, msg);
        return;
    }

    if ( target != m_olHover && !m_olPressed )
    {
        if ( !m_olHover && m_olUI )
            olui_mouse(m_olUI, -1, -1, OLUI_MOUSE_MOVE);   // olui's hover off
        if ( m_olHover )
            m_olHover->OLMouse(wxEVT_LEAVE_WINDOW, cx, cy, qual);
        m_olHover = target;
        target->OLMouse(wxEVT_ENTER_WINDOW, cx, cy, qual);
        if ( !m_olHover )
            return;     // the enter took it away
    }
    OLSetRmbTrap(true);

    if ( down )
    {
        // a second press soon after, near the first: a double click
        if ( button == m_olDownButton && DoubleClick(m_olDownSecs, m_olDownMicros, msg->Seconds, msg->Micros) &&
             abs(x - m_olDownX) < 4 && abs(y - m_olDownY) < 4 )
        {
            what = button == 1 ? wxEVT_LEFT_DCLICK : button == 2 ? wxEVT_RIGHT_DCLICK : wxEVT_MIDDLE_DCLICK;
            m_olDownSecs = m_olDownMicros = 0;
        }
        else
        {
            m_olDownSecs = msg->Seconds;
            m_olDownMicros = msg->Micros;
        }
        m_olDownX = x;
        m_olDownY = y;
        m_olDownButton = button;
        if ( target != (wxWindow *)this && target->AcceptsFocus() && target != m_olFocus )
            OLSetFocus(target);
        m_olPressed = target;
    }
    wxWindow *const was = target;
    const bool taken = target->OLMouse(what, cx, cy, qual);
    if ( what == wxEVT_RIGHT_UP && !taken && m_olWin )
    {
        // as Windows does: a right click nobody took asks for a context menu
        // (the event goes up to the parents until one takes it)
        wxContextMenuEvent ctx(wxEVT_CONTEXT_MENU, target->GetId(), target->ClientToScreen(wxPoint(cx, cy)));
        ctx.SetEventObject(target);
        target->HandleWindowEvent(ctx);
        if ( !m_olWin )
            return;
    }
    if ( up && m_olPressed == was )
    {
        m_olPressed = NULL;
        // the pointer may have left it while the button was down
        int nx, ny;
        wxWindow *now = OLWindowAt(x, y, &nx, &ny);
        if ( now != m_olHover && m_olHover )
        {
            m_olHover->OLMouse(wxEVT_LEAVE_WINDOW, cx, cy, qual);
            m_olHover = now;
            if ( now )
                now->OLMouse(wxEVT_ENTER_WINDOW, nx, ny, qual);
        }
    }
}

// the wx key code of an Amiga raw key that has no character
static int SpecialKey(unsigned raw)
{
    switch ( raw )
    {
        case 0x40: return WXK_SPACE;
        case 0x41: return WXK_BACK;
        case 0x42: return WXK_TAB;
        case 0x43: return WXK_NUMPAD_ENTER;
        case 0x44: return WXK_RETURN;
        case 0x45: return WXK_ESCAPE;
        case 0x46: return WXK_DELETE;
        case 0x47: return WXK_INSERT;
        case 0x48: return WXK_PAGEUP;
        case 0x49: return WXK_PAGEDOWN;
        case 0x4b: return WXK_F11;
        case 0x4c: return WXK_UP;
        case 0x4d: return WXK_DOWN;
        case 0x4e: return WXK_RIGHT;
        case 0x4f: return WXK_LEFT;
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
        case 0x55: case 0x56: case 0x57: case 0x58: case 0x59:
            return WXK_F1 + (int)(raw - 0x50);
        case 0x5f: return WXK_HELP;
        case 0x60: case 0x61: return WXK_SHIFT;
        case 0x62: return WXK_CAPITAL;
        case 0x63: return WXK_CONTROL;
        case 0x64: case 0x65: return WXK_ALT;
        case 0x66: return WXK_WINDOWS_LEFT;
        case 0x67: return WXK_WINDOWS_RIGHT;
        case 0x6f: return WXK_F12;
        case 0x70: return WXK_HOME;
        case 0x71: return WXK_END;
        case 0x0f: return WXK_NUMPAD0;
        case 0x1d: return WXK_NUMPAD1;
        case 0x1e: return WXK_NUMPAD2;
        case 0x1f: return WXK_NUMPAD3;
        case 0x2d: return WXK_NUMPAD4;
        case 0x2e: return WXK_NUMPAD5;
        case 0x2f: return WXK_NUMPAD6;
        case 0x3d: return WXK_NUMPAD7;
        case 0x3e: return WXK_NUMPAD8;
        case 0x3f: return WXK_NUMPAD9;
        case 0x3c: return WXK_NUMPAD_DECIMAL;
        case 0x4a: return WXK_NUMPAD_SUBTRACT;
        case 0x5e: return WXK_NUMPAD_ADD;
        case 0x5c: return WXK_NUMPAD_DIVIDE;
        case 0x5d: return WXK_NUMPAD_MULTIPLY;
        default: return 0;
    }
}

// the character the keymap gives the key (0: none, or more than one)
static unsigned MapKey(unsigned raw, unsigned qual, APTR prev)
{
    if ( !gs_keymapBase && !(gs_keymapBase = OpenLibrary((STRPTR)"keymap.library", 37)) )
        return 0;
    struct InputEvent ie;
    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = (UWORD)raw;
    ie.ie_Qualifier = (UWORD)qual;
    ie.ie_EventAddress = prev;
    UBYTE buf[4];
    if ( wxOL_MAPRAWKEY(&ie, (STRPTR)buf, (LONG)sizeof buf, NULL) != 1 )
        return 0;
    return buf[0];
}

void wxOLCloseKeymap()
{
    if ( gs_keymapBase )
        CloseLibrary(gs_keymapBase);
    gs_keymapBase = NULL;
}

void wxOLRunMenuItem(wxMenuItem *item, int checked);

// a menu item whose accelerator is the key (Audacity's menus have them)
#if wxUSE_MENUS && wxUSE_ACCEL
static wxMenuItem *FindAccel(wxMenu *menu, const wxKeyEvent& ev)
{
    for ( wxMenuItemList::compatibility_iterator n = menu->GetMenuItems().GetFirst(); n; n = n->GetNext() )
    {
        wxMenuItem *item = n->GetData();
        if ( item->GetSubMenu() )
        {
            if ( wxMenuItem *found = FindAccel(item->GetSubMenu(), ev) )
                return found;
            continue;
        }
        wxAcceleratorEntry *a = item->GetAccel();
        if ( !a )
            continue;
        int flags = 0;
        if ( ev.ControlDown() ) flags |= wxACCEL_CTRL;
        if ( ev.AltDown() ) flags |= wxACCEL_ALT;
        if ( ev.ShiftDown() ) flags |= wxACCEL_SHIFT;
        const bool match = a->GetKeyCode() == ev.GetKeyCode() && (a->GetFlags() & ~wxACCEL_RAW_CTRL) == flags;
        delete a;
        if ( match && item->IsEnabled() )
            return item;
    }
    return NULL;
}
#endif

void wxNonOwnedWindow::OLKeyMessage(struct IntuiMessage *msg)
{
    const unsigned raw = msg->Code & 0x7f;
    const bool up = (msg->Code & IECODE_UP_PREFIX) != 0;
    const unsigned qual = msg->Qualifier;
    wxOLLastQualifier = msg->Qualifier;

    // the key: a special one, else its character without qualifiers
    // (letters in capitals, as wx has them)
    int key = SpecialKey(raw);
    unsigned ch = MapKey(raw, qual, msg->IAddress);
    if ( !key )
    {
        unsigned plain = MapKey(raw, 0, NULL);
        if ( !plain )
            return;
        key = (plain >= 'a' && plain <= 'z') || (plain >= 0xe0 && plain <= 0xfe && plain != 0xf7) ? plain - 32 : plain;
    }

    wxKeyEvent ev(up ? wxEVT_KEY_UP : wxEVT_KEY_DOWN);
    ev.m_keyCode = key;
#if wxUSE_UNICODE
    ev.m_uniChar = key < WXK_START ? key : WXK_NONE;
#endif
    ev.m_shiftDown = (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
    ev.m_controlDown = (qual & (IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) != 0;
    ev.m_altDown = (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) != 0;
    ev.m_metaDown = (qual & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) != 0;
    ev.m_rawCode = raw;
    ev.m_rawFlags = qual;
    ev.m_x = msg->MouseX - m_olWin->BorderLeft;
    ev.m_y = msg->MouseY - m_olWin->BorderTop;
    ev.SetTimestamp(msg->Seconds * 1000 + msg->Micros / 1000);

    wxWindow *focus = m_olFocus ? m_olFocus : (wxWindow *)this;
    const bool native = focus->OLIsNative() && m_olUI && olui_focused(m_olUI);
    ev.SetEventObject(focus);
    ev.SetId(focus->GetId());

    if ( up )
    {
        if ( !focus->HandleWindowEvent(ev) && native )
            olui_amiga_event(m_olAmiga, msg);
        return;
    }

    // 1: the char hook (a dialog's Escape)
    wxKeyEvent hook(wxEVT_CHAR_HOOK, ev);
    hook.SetEventObject(focus);
    if ( focus->HandleWindowEvent(hook) && !hook.IsNextEventAllowed() )
        return;
    // 2: the key down; then olui's widget, when it has the keys
    if ( focus->HandleWindowEvent(ev) )
        return;
    if ( native && olui_amiga_event(m_olAmiga, msg) == OLUI_AMIGA_USED )
        return;
    // 3: the character
    wxKeyEvent cev(wxEVT_CHAR, ev);
    cev.SetEventObject(focus);
    if ( ch && !SpecialKey(raw) )
    {
        cev.m_keyCode = ch;
#if wxUSE_UNICODE
        cev.m_uniChar = ch;
#endif
    }
    else if ( key == WXK_SPACE || key == WXK_RETURN || key == WXK_TAB || key == WXK_BACK || key == WXK_ESCAPE )
    {
#if wxUSE_UNICODE
        cev.m_uniChar = key;
#endif
    }
    if ( !native && focus->HandleWindowEvent(cev) )
        return;
    // 4: the accelerators: the windows', then the menus'
#if wxUSE_ACCEL
    for ( wxWindow *w = focus; w; w = w->IsTopLevel() ? NULL : w->GetParent() )
    {
        wxAcceleratorTable *t = w->GetAcceleratorTable();
        int cmd = t && t->IsOk() ? t->GetCommand(ev) : -1;
        if ( cmd != -1 )
        {
            wxCommandEvent mev(wxEVT_MENU, cmd);
            mev.SetEventObject(this);
            HandleWindowEvent(mev);
            return;
        }
    }
#endif
#if wxUSE_MENUS && wxUSE_ACCEL
    if ( wxFrame *frame = wxDynamicCast(this, wxFrame) )
        if ( wxMenuBar *mb = frame->GetMenuBar() )
            for ( size_t i = 0; i < mb->GetMenuCount(); i++ )
                if ( mb->IsEnabledTop(i) )
                    if ( wxMenuItem *item = FindAccel(mb->GetMenu(i), ev) )
                    {
                        wxOLRunMenuItem(item, -1);
                        return;
                    }
#endif
    // 5: what wx does with what is left: Return presses the default button,
    // Tab goes to the next window
    if ( key == WXK_RETURN || key == WXK_NUMPAD_ENTER )
    {
        wxTopLevelWindow *tlw = wxDynamicCast(this, wxTopLevelWindow);
        wxWindow *def = tlw ? tlw->GetDefaultItem() : NULL;
#if wxUSE_BUTTON
        wxButton *b = def ? wxDynamicCast(def, wxButton) : NULL;
        if ( b && b->IsEnabled() && b->IsShownOnScreen() )
        {
            wxCommandEvent bev(wxEVT_BUTTON, b->GetId());
            bev.SetEventObject(b);
            b->HandleWindowEvent(bev);
            return;
        }
#endif
    }
    if ( key == WXK_TAB && !native )
    {
        int flags = ev.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward;
        if ( ev.ControlDown() )
            flags |= wxNavigationKeyEvent::WinChange;
        focus->Navigate(flags);
    }
}

void wxNonOwnedWindow::OLHandleMessage(struct IntuiMessage *msg)
{
    if ( !m_olWin || !m_olAmiga )
        return;
    m_olBusy = true;
    switch ( msg->Class )
    {
        case IDCMP_CLOSEWINDOW:
            m_olBusy = false;
            Close();
            break;
        case IDCMP_NEWSIZE:
            OLSized();
            break;
        case IDCMP_CHANGEWINDOW:
            OLSized();
            break;
        case IDCMP_REFRESHWINDOW:
            if ( m_olLayoutNeeded )
            {
                BeginRefresh(m_olWin);
                EndRefresh(m_olWin, TRUE);
                OLDrawPending();
            }
            else
                olui_amiga_event(m_olAmiga, msg);
            break;
        case IDCMP_ACTIVEWINDOW:
        case IDCMP_INACTIVEWINDOW:
        {
            m_olActive = msg->Class == IDCMP_ACTIVEWINDOW;
            wxActivateEvent ev(wxEVT_ACTIVATE, m_olActive, GetId());
            ev.SetEventObject(this);
            HandleWindowEvent(ev);
            if ( !m_olActive && m_olHover )
            {
                wxWindow *old = m_olHover;
                m_olHover = NULL;
                old->OLMouse(wxEVT_LEAVE_WINDOW, -1, -1, msg->Qualifier);
            }
            break;
        }
        case IDCMP_MENUPICK:
            m_olBusy = false;
            OLMenuPicked(msg->Code);
            break;
        case IDCMP_MOUSEMOVE:
        case IDCMP_MOUSEBUTTONS:
            OLMouseMessage(msg);
            break;
        case IDCMP_EXTENDEDMOUSE:
            // OS 3.2 gives the wheel this way, not as the raw keys 0x7a/0x7b
            // of NewMouse: each step is given as one of those
            if ( (msg->Code & wxOL_INTUIWHEELDATA) && msg->IAddress )
            {
                const int dy = ((const struct wxOLWheelData *)msg->IAddress)->WheelY;
                struct IntuiMessage step = *msg;
                step.Class = IDCMP_RAWKEY;
                step.Code = dy > 0 ? wxOL_RAW_WHEEL_DOWN : wxOL_RAW_WHEEL_UP;
                step.IAddress = NULL;
                for ( int n = wxMin(abs(dy), 8); n > 0; n-- )
                    OLMouseMessage(&step);
            }
            break;
        case IDCMP_RAWKEY:
            if ( msg->Code == wxOL_RAW_WHEEL_UP || msg->Code == wxOL_RAW_WHEEL_DOWN )
                OLMouseMessage(msg);
            else
                OLKeyMessage(msg);
            break;
        case IDCMP_INTUITICKS:
            olui_amiga_event(m_olAmiga, msg);
            break;
        default:
            break;
    }
    m_olBusy = false;
    OLRunQueue();
    if ( m_olCloseLater )
    {
        m_olCloseLater = false;
        OLClose();
    }
}

// ---- wxTopLevelWindow ------------------------------------------------------------------

void wxTopLevelWindowOpenLayout::Init()
{
    m_fullScreen = false;
    m_maximized = false;
}

bool wxTopLevelWindowOpenLayout::Create(wxWindow *parent, wxWindowID id, const wxString& title,
                                        const wxPoint& pos, const wxSize& size, long style,
                                        const wxString& name)
{
    m_title = title;
    if ( !wxNonOwnedWindow::Create(parent, id, pos, size, style, name) )
        return false;
    wxTopLevelWindows.Append(this);
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    m_hasBgCol = false;
    return true;
}

void wxTopLevelWindowOpenLayout::Maximize(bool maximize)
{
    struct Screen *scr = wxOLGetScreen();
    if ( !scr || maximize == m_maximized )
        return;
    if ( maximize )
    {
        m_restore = GetRect();
        const int top = scr->BarHeight + 1;
        SetSize(0, top, scr->Width, scr->Height - top);
    }
    else
        SetSize(m_restore);
    m_maximized = maximize;
}

bool wxTopLevelWindowOpenLayout::IsMaximized() const
{
    return m_maximized;
}

void wxTopLevelWindowOpenLayout::Iconize(bool iconize)
{
    // a window is put behind the others: Workbench has no iconified windows
    if ( iconize )
        Lower();
    else
        Raise();
}

bool wxTopLevelWindowOpenLayout::IsIconized() const
{
    return false;
}

void wxTopLevelWindowOpenLayout::Restore()
{
    if ( m_maximized )
        Maximize(false);
}

bool wxTopLevelWindowOpenLayout::ShowFullScreen(bool show, long WXUNUSED(style))
{
    if ( show == m_fullScreen )
        return true;
    m_fullScreen = show;
    Maximize(show);
    return true;
}

void wxTopLevelWindowOpenLayout::SetTitle(const wxString& title)
{
    m_title = title;
    if ( m_olWin && !(m_olWin->Flags & WFLG_BORDERLESS) )
    {
        // Intuition keeps the pointer: the old copy goes once the new is in
        std::string t = wxOLToLatin1(title);
        char *copy = t.empty() ? NULL : (char *)AllocVec(t.size() + 1, MEMF_ANY);
        if ( copy || t.empty() )
        {
            if ( copy )
                memcpy(copy, t.c_str(), t.size() + 1);
            SetWindowTitles(m_olWin, (CONST_STRPTR)copy, (CONST_STRPTR)~0);
            if ( m_olTitleText )
                FreeVec(m_olTitleText);
            m_olTitleText = copy;
        }
    }
}

bool wxTopLevelWindowOpenLayout::IsActive()
{
    return m_olWin && (m_olWin->Flags & WFLG_WINDOWACTIVE);
}

// ---- wxFrame ------------------------------------------------------------------------------

// (wxFrame's class info is in wx's common code)

bool wxFrame::Create(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos,
                     const wxSize& size, long style, const wxString& name)
{
    if ( !wxTopLevelWindow::Create(parent, id, title, pos, size, style, name) )
        return false;
    Bind(wxEVT_SIZE, &wxFrame::OLOnSize, this);
    return true;
}

wxFrame::~wxFrame()
{
    SendDestroyEvent();
#if wxUSE_MENUS
    // the strip goes before the window it is in
    if ( m_olWin && m_olWin->MenuStrip )
        ClearMenuStrip(m_olWin);
#endif
}

void wxFrame::OLOnSize(wxSizeEvent& event)
{
#if wxUSE_STATUSBAR
    PositionStatusBar();
#endif
#if wxUSE_TOOLBAR
    PositionToolBar();
#endif
    event.Skip();
}

#if wxUSE_MENUS
void wxFrame::SetMenuBar(wxMenuBar *menubar)
{
    if ( m_olWin && m_olWin->MenuStrip )
        ClearMenuStrip(m_olWin);
    wxFrameBase::SetMenuBar(menubar);
    if ( m_olWin )
    {
        if ( struct Menu *strip = OLGetMenuStrip() )
            SetMenuStrip(m_olWin, strip);
        // the right button: the window's without a strip, else the menus'
        m_olRmbTrap = m_olWin->MenuStrip != NULL;
        OLSetRmbTrap(false);
    }
}
#endif

void wxFrame::OLUpdateMenus()
{
#if wxUSE_MENUS
    // what changed in the menus since (enabled, checked, new items) is put
    // in the strip while no menu is showing
    if ( m_frameMenuBar && m_olWin && m_frameMenuBar->OLDirty() )
    {
        if ( m_olWin->MenuStrip )
            ClearMenuStrip(m_olWin);
        if ( struct Menu *strip = m_frameMenuBar->OLGetStrip(m_olWin) )
            SetMenuStrip(m_olWin, strip);
    }
#endif
}

struct Menu *wxFrame::OLGetMenuStrip() const
{
#if wxUSE_MENUS
    if ( m_frameMenuBar && m_olWin )
        return m_frameMenuBar->OLGetStrip(m_olWin);
#endif
    return NULL;
}

void wxFrame::OLMenuPicked(unsigned short code)
{
#if wxUSE_MENUS
    if ( m_frameMenuBar )
        m_frameMenuBar->OLPicked(code);
#else
    (void)code;
#endif
}

wxPoint wxFrame::GetClientAreaOrigin() const
{
    wxPoint pt = wxFrameBase::GetClientAreaOrigin();
#if wxUSE_TOOLBAR
    if ( m_frameToolBar && m_frameToolBar->IsShown() )
    {
        if ( m_frameToolBar->GetWindowStyleFlag() & wxTB_VERTICAL )
            pt.x += m_frameToolBar->GetSize().x;
        else
            pt.y += m_frameToolBar->GetSize().y;
    }
#endif
    return pt;
}

void wxFrame::DoGetClientSize(int *width, int *height) const
{
    wxTopLevelWindow::DoGetClientSize(width, height);
#if wxUSE_STATUSBAR
    if ( m_frameStatusBar && m_frameStatusBar->IsShown() && height )
        *height -= m_frameStatusBar->GetSize().y;
#endif
#if wxUSE_TOOLBAR
    if ( m_frameToolBar && m_frameToolBar->IsShown() )
    {
        if ( m_frameToolBar->GetWindowStyleFlag() & wxTB_VERTICAL )
        {
            if ( width )
                *width -= m_frameToolBar->GetSize().x;
        }
        else if ( height )
            *height -= m_frameToolBar->GetSize().y;
    }
#endif
    if ( width && *width < 0 ) *width = 0;
    if ( height && *height < 0 ) *height = 0;
}

void wxFrame::DoSetClientSize(int width, int height)
{
#if wxUSE_STATUSBAR
    if ( m_frameStatusBar && m_frameStatusBar->IsShown() )
        height += m_frameStatusBar->GetSize().y;
#endif
#if wxUSE_TOOLBAR
    if ( m_frameToolBar && m_frameToolBar->IsShown() )
    {
        if ( m_frameToolBar->GetWindowStyleFlag() & wxTB_VERTICAL )
            width += m_frameToolBar->GetSize().x;
        else
            height += m_frameToolBar->GetSize().y;
    }
#endif
    wxTopLevelWindow::DoSetClientSize(width, height);
}

#if wxUSE_STATUSBAR
void wxFrame::PositionStatusBar()
{
    if ( m_frameStatusBar )
    {
        const wxSize size = GetClientSize();
        int h = m_frameStatusBar->GetSize().y;
        if ( h <= 0 )
            h = m_frameStatusBar->GetBestSize().y;
        m_frameStatusBar->SetSize(0, size.y, size.x, h);
    }
}
#endif

#if wxUSE_TOOLBAR
void wxFrame::PositionToolBar()
{
    if ( m_frameToolBar )
    {
        const wxSize size = GetClientSize();
        int tw, th;
        m_frameToolBar->GetSize(&tw, &th);
        if ( m_frameToolBar->GetWindowStyleFlag() & wxTB_VERTICAL )
            m_frameToolBar->SetSize(-tw, 0, tw, size.y);
        else
            m_frameToolBar->SetSize(0, -th, size.x, th);
    }
}
#endif

// ---- wxDialog ---------------------------------------------------------------------------------

// (wxDialog's class info is in wx's common code)

bool wxDialog::Create(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos,
                      const wxSize& size, long style, const wxString& name)
{
    SetExtraStyle(GetExtraStyle() | wxTOPLEVEL_EX_DIALOG);
    // a dialog goes over its parent
    wxWindow *p = GetParentForModalDialog(parent, style);
    return wxTopLevelWindow::Create(p ? p : parent, id, title, pos, size, style, name);
}

wxDialog::~wxDialog()
{
    SendDestroyEvent();
    if ( m_isModal )
        EndModal(wxID_CANCEL);
}

bool wxDialog::Show(bool show)
{
    if ( show == IsShown() )
        return false;
    if ( show && CanDoLayoutAdaptation() )
        DoLayoutAdaptation();
    if ( show && GetParent() && !IsModal() && GetPosition() == wxPoint(0, 0) )
        CentreOnParent();
    if ( !show && m_isModal )
        EndModal(wxID_CANCEL);
    return wxTopLevelWindow::Show(show);
}

int wxDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();
    wxCHECK_MSG(!m_isModal, GetReturnCode(), "the dialog is already modal");

    wxWindow *parent = GetParentForModalDialog();
    if ( parent && parent != this )
        CentreOnParent();

    // the other windows take no input while it is up
    wxWindowDisabler disabler(this);
    m_isModal = true;
    SetReturnCode(0);
    if ( !IsShown() )
        wxTopLevelWindow::Show(true);
    InitDialog();

    wxGUIEventLoop loop;
    m_modalLoop = &loop;
    loop.Run();
    m_modalLoop = NULL;
    m_isModal = false;
    return GetReturnCode();
}

void wxDialog::EndModal(int retCode)
{
    SetReturnCode(retCode);
    if ( !m_isModal )
    {
        Hide();
        return;
    }
    m_isModal = false;
    if ( m_modalLoop )
        m_modalLoop->Exit();
    wxTopLevelWindow::Show(false);
}

// ---- wxPopupWindow --------------------------------------------------------------------------

// (wxPopupWindow's class info is in wx's common code)

bool wxPopupWindow::Create(wxWindow *parent, int flags)
{
    return wxNonOwnedWindow::Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, flags | wxPOPUP_WINDOW,
                                    wxASCII_STR("popup"));
}

unsigned long wxPopupWindow::OLGetFlags() const
{
    return WFLG_BORDERLESS | WFLG_REPORTMOUSE | WFLG_SIMPLE_REFRESH | WFLG_RMBTRAP;
}
