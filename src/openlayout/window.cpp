/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/window.cpp
// Purpose:     wxWindow: a positioned element in its top-level window's
//              GFOpenLayout UI, drawn by olui (a native control) or by wx
//              through a picture olui asks for (EVT_PAINT)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
    #include "wx/nonownedwnd.h"
    #include "wx/toplevel.h"
    #include "wx/dcclient.h"
    #include "wx/menu.h"
    #include "wx/log.h"
    #include "wx/settings.h"
#endif

#include "wx/tooltip.h"
#include "wx/scrolbar.h"
#include "wx/dnd.h"

#include <intuition/intuition.h>
#include <graphics/text.h>

#include "private.h"

// the paint being given now (dc.cpp's window DCs draw there)
struct wxOLPaintContext
{
    wxWindow *win;
    struct RastPort *rp;
    int x, y, w, h;
};
wxOLPaintContext *wxOLCurrentPaint = NULL;

// the window with the mouse captured
static wxWindow *gs_capture = NULL;

// (wxWindow's class info is in wx's common code)

void wxWindowOpenLayout::Init()
{
    m_olTLW = NULL;
    m_olEl = NULL;
    m_olImage = 0;
    m_olPainting = false;
    for ( int i = 0; i < 2; i++ )
    {
        m_scroll[i].pos = m_scroll[i].thumb = m_scroll[i].range = 0;
        m_olBar[i] = NULL;
    }
}

wxWindowOpenLayout::~wxWindowOpenLayout()
{
    SendDestroyEvent();
    if ( gs_capture == this )
        gs_capture = NULL;
    for ( int i = 0; i < 2; i++ )
        wxDELETE(m_olBar[i]);
    DestroyChildren();
    OLDropElement();
    if ( m_olTLW && m_olTLW != (wxNonOwnedWindow *)this )
        m_olTLW->OLForget((wxWindow *)this);
    if ( GetParent() )
        GetParent()->RemoveChild(this);
}

bool wxWindowOpenLayout::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                                long style, const wxString& name)
{
    wxCHECK_MSG(parent, false, "a child window needs a parent");
    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    parent->AddChild(this);
    m_rect = wxRect(pos.x == wxDefaultCoord ? 0 : pos.x, pos.y == wxDefaultCoord ? 0 : pos.y,
                    size.x == wxDefaultCoord ? 20 : size.x, size.y == wxDefaultCoord ? 20 : size.y);
    m_olTLW = parent->OLGetTLW();
    OLMakeElement();
    return true;
}

// ---- the element -------------------------------------------------------------------

struct olui *wxWindowOpenLayout::OLGetUI() const
{
    return m_olTLW ? m_olTLW->OLGetOwnUI() : NULL;
}

wxString wxWindowOpenLayout::OLId() const
{
    return wxString::Format("w%lx", (unsigned long)(wxUIntPtr)this);
}

wxString wxWindowOpenLayout::OLGetMarkup() const
{
    // a box wx draws: the picture olui asks for is its background image
    wxString m = "<div id=" + OLId();
    if ( GetToolTip() && !GetToolTip()->GetTip().empty() )
        m += " title=\"" + wxOLEscape(GetToolTip()->GetTip()) + "\"";
    return m + "></div>";
}

void wxWindowOpenLayout::OLMakeElement()
{
    if ( m_olEl || !m_olTLW )
        return;
    struct olui *ui = OLGetUI();
    if ( !ui )
        return;
    wxWindow *parent = GetParent();
    struct olui_el *container = parent ? parent->OLGetChildContainer() : NULL;
    if ( parent && parent != (wxWindow *)m_olTLW && !container )
        return;     // the parent has no element yet
    if ( (!OLIsNative() || OLWantsImage()) && !OLOwnDrawn() && !m_olImage )
        m_olImage = m_olTLW->OLAllocImage((wxWindow *)this);
    const wxString ml = OLGetMarkup();
    // the outer tag's own style (text-align, display:flex, padding...),
    // kept for OLPlace(), which replaces the attribute with its placement
    m_olMarkupStyle.clear();
    const wxString tag = ml.BeforeFirst('>');
    for ( const char *q : { " style='", " style=\"" } )
    {
        const int at = tag.Find(q);
        if ( at == wxNOT_FOUND )
            continue;
        const wxString rest = tag.Mid(at + 8);
        const int end = rest.Find(q[7]);
        if ( end != wxNOT_FOUND )
            m_olMarkupStyle = rest.Left(end);
        break;
    }
    const std::string markup = wxOLToUTF8(ml);
    char err[128];
    m_olEl = olui_add(ui, container, markup.c_str(), err, sizeof err);
    if ( !m_olEl )
    {
        wxLogDebug("olui_add(%s): %s", markup.c_str(), err);
        return;
    }
    olui_set_user(m_olEl, this);
    OLPlace();
    if ( !IsShown() )
        olui_set_hidden(ui, m_olEl, 1);
    if ( !IsThisEnabled() )
        olui_set_disabled(ui, m_olEl, 1);
    OLElementMade();
    if ( OLOwnDrawn() )
        m_olTLW->OLSetOwn((wxWindow *)this, true);
    m_olTLW->OLNeedLayout();
}

void wxWindowOpenLayout::OLDropElement()
{
    if ( !m_olEl )
        return;
    struct olui *ui = OLGetUI();
    // what is in it goes with it: its children's elements are forgotten
    for ( wxWindowList::compatibility_iterator n = GetChildren().GetFirst(); n; n = n->GetNext() )
        if ( !n->GetData()->IsTopLevel() )
            n->GetData()->OLDropElement();
    for ( int i = 0; i < 2; i++ )
        if ( m_olBar[i] )
            m_olBar[i]->OLDropElement();
    // (a window drawing itself is given back first: olui keeps its element)
    if ( m_olTLW && m_olTLW != (wxNonOwnedWindow *)this )
        m_olTLW->OLSetOwn((wxWindow *)this, false);
    if ( ui )
        olui_remove(ui, m_olEl);
    m_olEl = NULL;
    if ( m_olImage && m_olTLW )
        m_olTLW->OLFreeImage(m_olImage);
    m_olImage = 0;
}

void wxWindowOpenLayout::OLRecreate()
{
    if ( !m_olEl )
        return;
    OLDropElement();
    OLMakeElement();
    // and its children again in it
    for ( wxWindowList::compatibility_iterator n = GetChildren().GetFirst(); n; n = n->GetNext() )
    {
        wxWindow *c = n->GetData();
        if ( !c->IsTopLevel() )
        {
            c->OLMakeElement();
            // a child's children too
            c->OLRecreate();
        }
    }
    // its scroll bars over them
    for ( int i = 0; i < 2; i++ )
        if ( m_olBar[i] )
            m_olBar[i]->OLMakeElement();
}

void wxWindowOpenLayout::OLPlace()
{
    struct olui *ui = OLGetUI();
    if ( !m_olEl || !ui )
        return;
    wxString decls = wxString::Format("position:absolute;left:0;top:0;width:%dpx;height:%dpx;margin:0;"
                                      "box-sizing:border-box",
                                      wxMax(m_rect.width, 0), wxMax(m_rect.height, 0));
    // the markup's own declarations first: the placement's win over them
    if ( !m_olMarkupStyle.empty() )
        decls = m_olMarkupStyle + ";" + decls;
    if ( OLOwnDrawn() )
    {
        // a plain box (olui notes where a box with a background is drawn,
        // and copies its buffer around it): black unless a colour was set
        const wxColour c = UseBgCol() ? GetBackgroundColour() : *wxBLACK;
        decls += wxString::Format(";padding:0;border:0;overflow:hidden;background-color:#%02x%02x%02x",
                                  c.Red(), c.Green(), c.Blue());
    }
    else if ( !OLIsNative() )
    {
        decls += wxString::Format(";padding:0;border:0;overflow:hidden;background-image:url(%d)", m_olImage);
        if ( UseBgCol() )
        {
            // background-color: the background shorthand would take the
            // picture (background-image) away again
            const wxColour c = GetBackgroundColour();
            decls += wxString::Format(";background-color:#%02x%02x%02x", c.Red(), c.Green(), c.Blue());
        }
    }
    else
    {
        if ( m_hasFgCol )
        {
            const wxColour c = GetForegroundColour();
            decls += wxString::Format(";color:#%02x%02x%02x", c.Red(), c.Green(), c.Blue());
        }
        // a font given with SetFont: olui draws the control's text at its
        // size, bold and fixed width as asked (the theme's own faces)
        if ( m_hasFont && m_font.IsOk() )
        {
            struct TextFont *tf = m_font.GetTextFont();
            if ( tf )
                decls += wxString::Format(";font-size:%dpx", (int)tf->tf_YSize);
            // a named face first (olui opens it through diskfont), then the
            // theme's fixed face for a fixed one
            const wxFontFamily fam = m_font.GetFamily();
            const bool fixed = m_font.IsFixedWidth() || fam == wxFONTFAMILY_MODERN || fam == wxFONTFAMILY_TELETYPE;
            wxString face = m_font.GetFaceName();
            face.Replace("\\", "");
            face.Replace("\"", "");
            if ( !face.empty() )
                decls += ";font-family:\"" + face + "\"" + (fixed ? ",monospace" : "");
            else if ( fixed )
                decls += ";font-family:monospace";
            if ( m_font.GetWeight() >= wxFONTWEIGHT_BOLD )
                decls += ";font-weight:bold";
        }
    }
    const std::string d = wxOLToUTF8(decls);
    olui_set_style(ui, m_olEl, d.c_str());
    // a frame's client area starts below its toolbar
    wxPoint o = GetParent() ? GetParent()->GetClientAreaOrigin() : wxPoint(0, 0);
    olui_set_place(ui, m_olEl, m_rect.x + o.x, m_rect.y + o.y);
}

wxRect wxWindowOpenLayout::OLGetBoxInTLW() const
{
    if ( IsTopLevel() )
    {
        int w, h;
        DoGetClientSize(&w, &h);
        return wxRect(0, 0, w, h);
    }
    wxRect r = m_rect;
    for ( const wxWindow *p = GetParent(); p; p = p->GetParent() )
    {
        const wxPoint o = p->GetClientAreaOrigin();
        r.x += o.x;
        r.y += o.y;
        if ( p->IsTopLevel() )
            break;
        r.x += p->m_rect.x;
        r.y += p->m_rect.y;
    }
    return r;
}

// ---- painting ------------------------------------------------------------------------

void wxWindowOpenLayout::OLPaint(struct RastPort *rp, int x, int y, int w, int h)
{
    if ( m_olPainting || !IsShownOnScreen() )
        return;
    m_olPainting = true;
    wxOLPaintContext ctx = { (wxWindow *)this, rp, x, y, w, h };
    wxOLPaintContext *was = wxOLCurrentPaint;
    wxOLCurrentPaint = &ctx;
    m_updateRegion = wxRegion(0, 0, w, h);

    if ( GetBackgroundStyle() != wxBG_STYLE_PAINT && GetBackgroundStyle() != wxBG_STYLE_TRANSPARENT )
    {
        wxClientDC dc((wxWindow *)this);
        wxEraseEvent erase(GetId(), &dc);
        erase.SetEventObject(this);
        if ( !HandleWindowEvent(erase) && UseBgCol() )
        {
            dc.SetBackground(wxBrush(GetBackgroundColour()));
            dc.Clear();
        }
    }
    wxNcPaintEvent nc((wxWindow *)this);
    HandleWindowEvent(nc);
    wxPaintEvent paint((wxWindow *)this);
    HandleWindowEvent(paint);

    m_updateRegion.Clear();
    wxOLCurrentPaint = was;
    m_olPainting = false;
}

void wxWindowOpenLayout::Refresh(bool WXUNUSED(eraseBackground), const wxRect *rect)
{
    struct olui *ui = OLGetUI();
    if ( !m_olEl || !ui )
    {
        if ( IsTopLevel() && m_olTLW )
            m_olTLW->OLInvalidate((wxWindow *)this, rect);
        return;
    }
    if ( rect )
    {
        wxRect box = OLGetBoxInTLW();
        olui_rect part = { box.x + rect->x, box.y + rect->y, rect->width, rect->height };
        olui_repaint(ui, m_olEl, &part);
    }
    else
        olui_repaint(ui, m_olEl, NULL);
    m_olTLW->OLInvalidate((wxWindow *)this, rect);
}

void wxWindowOpenLayout::Update()
{
    if ( m_olTLW )
        m_olTLW->OLDrawPending();
}

// ---- place and size ------------------------------------------------------------------

void wxWindowOpenLayout::DoGetPosition(int *x, int *y) const
{
    if ( x ) *x = m_rect.x;
    if ( y ) *y = m_rect.y;
}

void wxWindowOpenLayout::DoGetSize(int *width, int *height) const
{
    if ( width ) *width = m_rect.width;
    if ( height ) *height = m_rect.height;
}

// the scroll bars' width
static const int wxOL_BAR = 16;

void wxWindowOpenLayout::DoGetClientSize(int *width, int *height) const
{
    // less the scroll bars it shows
    if ( width ) *width = wxMax(0, m_rect.width - (OLBarShown(1) ? wxOL_BAR : 0));
    if ( height ) *height = wxMax(0, m_rect.height - (OLBarShown(0) ? wxOL_BAR : 0));
}

void wxWindowOpenLayout::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    if ( x == wxDefaultCoord && !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
        x = m_rect.x;
    if ( y == wxDefaultCoord && !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
        y = m_rect.y;
    if ( width == wxDefaultCoord )
        width = (sizeFlags & wxSIZE_AUTO_WIDTH) ? GetBestSize().x : m_rect.width;
    if ( height == wxDefaultCoord )
        height = (sizeFlags & wxSIZE_AUTO_HEIGHT) ? GetBestSize().y : m_rect.height;
    DoMoveWindow(x, y, width, height);
}

void wxWindowOpenLayout::DoMoveWindow(int x, int y, int width, int height)
{
    if ( width < 0 ) width = 0;
    if ( height < 0 ) height = 0;
    const bool sized = width != m_rect.width || height != m_rect.height;
    const bool moved = x != m_rect.x || y != m_rect.y;
    m_rect = wxRect(x, y, width, height);
    if ( !sized && !moved )
        return;
    OLPlace();
    if ( m_olTLW )
        m_olTLW->OLNeedLayout();
    if ( sized )
        OLUpdateBars();
    if ( sized )
    {
        wxSizeEvent ev(wxSize(width, height), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
    if ( moved )
    {
        wxMoveEvent ev(wxPoint(x, y), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
}

void wxWindowOpenLayout::DoSetClientSize(int width, int height)
{
    DoSetSize(m_rect.x, m_rect.y, width, height, wxSIZE_USE_EXISTING);
}

void wxWindowOpenLayout::DoClientToScreen(int *x, int *y) const
{
    wxRect box = OLGetBoxInTLW();
    int sx = 0, sy = 0;
    struct Window *w = m_olTLW ? m_olTLW->OLGetWindow() : NULL;
    if ( w )
    {
        sx = w->LeftEdge + w->BorderLeft;
        sy = w->TopEdge + w->BorderTop;
    }
    if ( x ) *x += box.x + sx;
    if ( y ) *y += box.y + sy;
}

void wxWindowOpenLayout::DoScreenToClient(int *x, int *y) const
{
    int ox = 0, oy = 0;
    DoClientToScreen(&ox, &oy);
    if ( x ) *x -= ox;
    if ( y ) *y -= oy;
}

// ---- showing, enabling, focus ------------------------------------------------------------

bool wxWindowOpenLayout::Show(bool show)
{
    if ( !wxWindowBase::Show(show) )
        return false;
    struct olui *ui = OLGetUI();
    if ( m_olEl && ui )
    {
        olui_set_hidden(ui, m_olEl, !show);
        m_olTLW->OLNeedLayout();
    }
    if ( !show && m_olTLW && m_olTLW->OLGetFocus() == (wxWindow *)this )
        m_olTLW->OLSetFocus(NULL);
    return true;
}

void wxWindowOpenLayout::DoEnable(bool enable)
{
    struct olui *ui = OLGetUI();
    if ( m_olEl && ui )
        olui_set_disabled(ui, m_olEl, !enable);
}

void wxWindowOpenLayout::SetFocus()
{
    if ( m_olTLW )
        m_olTLW->OLSetFocus((wxWindow *)this);
}

wxWindow *wxWindowBase::DoFindFocus()
{
    wxWindow *active = wxGetActiveWindow();
    wxNonOwnedWindow *tlw = active ? wxDynamicCast(active, wxNonOwnedWindow) : NULL;
    if ( !tlw && wxTheApp )
        tlw = wxDynamicCast(wxTheApp->GetTopWindow(), wxNonOwnedWindow);
    return tlw ? tlw->OLGetFocus() : NULL;
}

void wxWindowOpenLayout::Raise()
{
    struct olui *ui = OLGetUI();
    if ( m_olEl && ui )
    {
        // last among its parent's: drawn over the others
        OLRecreate();
    }
}

void wxWindowOpenLayout::Lower()
{
}

bool wxWindowOpenLayout::Reparent(wxWindowBase *newParent)
{
    wxWindow *np = (wxWindow *)newParent;
    // wx does nothing for the parent it has (Audacity docks its toolbars
    // that way), so its element stays as it is
    if ( np == GetParent() )
        return false;
    OLDropElement();
    if ( !wxWindowBase::Reparent(newParent) )
    {
        OLMakeElement();
        OLRehomeChildren();
        return false;
    }
    wxNonOwnedWindow *tlw = np ? np->OLGetTLW() : NULL;
    if ( tlw != m_olTLW && m_olTLW )
        m_olTLW->OLForget((wxWindow *)this);
    m_olTLW = tlw;
    OLMakeElement();
    OLRehomeChildren();
    return true;
}

void wxWindowOpenLayout::OLRehomeChildren()
{
    for ( wxWindowList::compatibility_iterator n = GetChildren().GetFirst(); n; n = n->GetNext() )
    {
        wxWindow *c = n->GetData();
        if ( c->IsTopLevel() )
            continue;
        if ( c->m_olTLW != m_olTLW )
        {
            if ( c->m_olTLW )
                c->m_olTLW->OLForget(c);
            c->m_olTLW = m_olTLW;
        }
        c->OLMakeElement();
        c->OLRehomeChildren();
    }
    for ( int i = 0; i < 2; i++ )
    {
        wxWindow *b = (wxWindow *)m_olBar[i];
        if ( !b )
            continue;
        if ( b->m_olTLW != m_olTLW )
        {
            if ( b->m_olTLW )
                b->m_olTLW->OLForget(b);
            b->m_olTLW = m_olTLW;
        }
        b->OLMakeElement();
    }
}

void wxWindowOpenLayout::WarpPointer(int WXUNUSED(x), int WXUNUSED(y))
{
}

// ---- the mouse ---------------------------------------------------------------------------

void wxWindowOpenLayout::DoCaptureMouse()
{
    gs_capture = (wxWindow *)this;
}

void wxWindowOpenLayout::DoReleaseMouse()
{
    if ( gs_capture == (wxWindow *)this )
        gs_capture = NULL;
}

wxWindow *wxWindowBase::GetCapture()
{
    return gs_capture;
}

bool wxWindowOpenLayout::SetCursor(const wxCursor& cursor)
{
    return wxWindowBase::SetCursor(cursor);
}

// mouse events wx gets for a window it draws, x, y its client coordinates
bool wxWindowOpenLayout::OLMouse(int what, int x, int y, unsigned qualifiers)
{
    wxMouseEvent ev((wxEventType)what);
    ev.SetEventObject(this);
    ev.SetId(GetId());
    ev.m_x = x;
    ev.m_y = y;
    ev.SetShiftDown((qualifiers & 0x03) != 0);
    ev.SetAltDown((qualifiers & 0x30) != 0);
    ev.SetControlDown((qualifiers & 0x08) != 0);
    ev.SetMetaDown((qualifiers & 0xc0) != 0);
    ev.SetLeftDown((qualifiers & 0x4000) != 0);
    ev.SetRightDown((qualifiers & 0x2000) != 0);
    ev.SetMiddleDown((qualifiers & 0x1000) != 0);
    ev.SetTimestamp(0);
    if ( what == wxEVT_MOUSEWHEEL )
    {
        ev.m_wheelRotation = (qualifiers & 0x10000) ? -120 : 120;
        ev.m_wheelDelta = 120;
        ev.m_linesPerAction = 3;
        ev.m_wheelAxis = wxMOUSE_WHEEL_VERTICAL;
    }
    if ( what == wxEVT_LEFT_DCLICK || what == wxEVT_RIGHT_DCLICK )
        ev.SetLeftDown(what == wxEVT_LEFT_DCLICK);
    if ( what == wxEVT_LEFT_DOWN )
        ev.SetLeftDown(true);
    else if ( what == wxEVT_LEFT_UP )
        ev.SetLeftDown(false);
    else if ( what == wxEVT_RIGHT_DOWN )
        ev.SetRightDown(true);
    else if ( what == wxEVT_RIGHT_UP )
        ev.SetRightDown(false);
    return HandleWindowEvent(ev);
}

bool wxWindowOpenLayout::OLKey(unsigned WXUNUSED(rawkey), unsigned WXUNUSED(code), unsigned WXUNUSED(qualifiers),
                               bool WXUNUSED(up), bool WXUNUSED(vanilla))
{
    // the top-level window makes the key events (toplevel.cpp)
    return false;
}

void wxWindowOpenLayout::OLHandleEvent(const struct olui_event& WXUNUSED(ev))
{
}

// ---- fonts and text ------------------------------------------------------------------------

bool wxWindowOpenLayout::SetFont(const wxFont& font)
{
    if ( !wxWindowBase::SetFont(font) )
        return false;
    // a control's element takes the font in its style (OLPlace)
    if ( OLIsNative() )
        OLPlace();
    return true;
}

int wxWindowOpenLayout::GetCharHeight() const
{
    wxFont f = GetFont();
    struct TextFont *tf = f.IsOk() ? f.GetTextFont() : NULL;
    if ( !tf )
        tf = wxOLScreenFont();
    return tf ? tf->tf_YSize : 8;
}

int wxWindowOpenLayout::GetCharWidth() const
{
    wxFont f = GetFont();
    struct TextFont *tf = f.IsOk() ? f.GetTextFont() : NULL;
    if ( !tf )
        tf = wxOLScreenFont();
    return wxOLCharWidth(tf, 0);
}

void wxWindowOpenLayout::DoGetTextExtent(const wxString& string, int *x, int *y, int *descent,
                                         int *externalLeading, const wxFont *theFont) const
{
    wxFont f = theFont && theFont->IsOk() ? *theFont : GetFont();
    struct TextFont *tf = f.IsOk() ? f.GetTextFont() : NULL;
    int w, h, d;
    wxOLTextExtent(tf, 0, string, &w, &h, &d);
    if ( x ) *x = w;
    if ( y ) *y = h;
    if ( descent ) *descent = d;
    if ( externalLeading ) *externalLeading = 0;
}

// ---- scrolling ----------------------------------------------------------------------------

// shown (wx's generic scrolling): a range over the thumb, or -1 (wxSHOW_SB_ALWAYS
// with nothing to scroll), a range of 0 hides it
bool wxWindowOpenLayout::OLBarShown(int i) const
{
    if ( OLIsNative() || IsTopLevel() )
        return false;
    const ScrollInfo& s = m_scroll[i];
    return s.range < 0 || (s.range > 0 && (s.thumb < s.range || HasFlag(wxALWAYS_SHOW_SB)));
}

// the bars made, placed and set as m_scroll says
void wxWindowOpenLayout::OLUpdateBars()
{
    int cw, ch;
    DoGetClientSize(&cw, &ch);
    for ( int i = 0; i < 2; i++ )
    {
        wxScrollBar *b = m_olBar[i];
        if ( !OLBarShown(i) )
        {
            if ( b && b->IsShown() )
                b->Hide();
            continue;
        }
        if ( !b )
        {
            b = new wxScrollBar((wxWindow *)this, wxID_ANY, wxPoint(i ? cw : 0, i ? 0 : ch),
                                wxSize(i ? wxOL_BAR : cw, i ? ch : wxOL_BAR), i ? wxSB_VERTICAL : wxSB_HORIZONTAL);
            // not a child: sizers, ScrollWindow and tab order leave it be
            GetChildren().DeleteObject((wxWindow *)b);
            b->Bind(wxEVT_SCROLL_TOP, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_BOTTOM, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_LINEUP, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_LINEDOWN, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_PAGEUP, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_PAGEDOWN, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_THUMBTRACK, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_THUMBRELEASE, &wxWindowOpenLayout::OLBarScrolled, this);
            b->Bind(wxEVT_SCROLL_CHANGED, &wxWindowOpenLayout::OLBarScrolled, this);
            m_olBar[i] = b;
        }
        else
        {
            b->SetSize(i ? cw : 0, i ? 0 : ch, i ? wxOL_BAR : cw, i ? ch : wxOL_BAR);
            if ( !b->IsShown() )
                b->Show();
        }
        const ScrollInfo& s = m_scroll[i];
        b->SetScrollbar(s.pos, wxMax(s.thumb, 0), wxMax(s.range, 0), wxMax(s.thumb, 1));
    }
}

// the bar's scroll event, the window's to scroll by (wxScrollWinEvent)
void wxWindowOpenLayout::OLBarScrolled(wxScrollEvent& event)
{
    const wxEventType t = event.GetEventType();
    wxEventType w;
    if ( t == wxEVT_SCROLL_TOP ) w = wxEVT_SCROLLWIN_TOP;
    else if ( t == wxEVT_SCROLL_BOTTOM ) w = wxEVT_SCROLLWIN_BOTTOM;
    else if ( t == wxEVT_SCROLL_LINEUP ) w = wxEVT_SCROLLWIN_LINEUP;
    else if ( t == wxEVT_SCROLL_LINEDOWN ) w = wxEVT_SCROLLWIN_LINEDOWN;
    else if ( t == wxEVT_SCROLL_PAGEUP ) w = wxEVT_SCROLLWIN_PAGEUP;
    else if ( t == wxEVT_SCROLL_PAGEDOWN ) w = wxEVT_SCROLLWIN_PAGEDOWN;
    else if ( t == wxEVT_SCROLL_THUMBTRACK ) w = wxEVT_SCROLLWIN_THUMBTRACK;
    else if ( t == wxEVT_SCROLL_THUMBRELEASE ) w = wxEVT_SCROLLWIN_THUMBRELEASE;
    else
        return;     // CHANGED: wx has no window event for it
    const int orient = event.GetOrientation();
    wxScrollWinEvent e(w, event.GetPosition(), orient);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

void wxWindowOpenLayout::SetScrollbar(int orient, int pos, int thumbVisible, int range, bool WXUNUSED(refresh))
{
    const int i = orient == wxHORIZONTAL ? 0 : 1;
    ScrollInfo& s = m_scroll[i];
    if ( s.pos == pos && s.thumb == thumbVisible && s.range == range )
        return;
    const bool was = OLBarShown(i);
    s.pos = pos;
    s.thumb = thumbVisible;
    s.range = range;
    OLUpdateBars();
    if ( was != OLBarShown(i) )
    {
        // the client area changed, as when another system's window gets or
        // loses a scroll bar
        if ( m_olTLW )
            m_olTLW->OLNeedLayout();
        wxSizeEvent ev(GetSize(), GetId());
        ev.SetEventObject(this);
        HandleWindowEvent(ev);
    }
}

void wxWindowOpenLayout::SetScrollPos(int orient, int pos, bool WXUNUSED(refresh))
{
    const int i = orient == wxHORIZONTAL ? 0 : 1;
    m_scroll[i].pos = pos;
    if ( m_olBar[i] )
        m_olBar[i]->SetThumbPosition(pos);
}

int wxWindowOpenLayout::GetScrollPos(int orient) const { return m_scroll[orient == wxHORIZONTAL ? 0 : 1].pos; }
int wxWindowOpenLayout::GetScrollThumb(int orient) const { return m_scroll[orient == wxHORIZONTAL ? 0 : 1].thumb; }
int wxWindowOpenLayout::GetScrollRange(int orient) const { return m_scroll[orient == wxHORIZONTAL ? 0 : 1].range; }

void wxWindowOpenLayout::ScrollWindow(int dx, int dy, const wxRect *WXUNUSED(rect))
{
    // the children move; what wx draws is drawn again
    for ( wxWindowList::compatibility_iterator n = GetChildren().GetFirst(); n; n = n->GetNext() )
    {
        wxWindow *c = n->GetData();
        if ( c->IsTopLevel() )
            continue;
        wxPoint p = c->GetPosition();
        c->Move(p.x + dx, p.y + dy);
    }
    Refresh();
}

// ---- menus, tooltips, drag and drop ----------------------------------------------------------

#if wxUSE_MENUS
bool wxOLPopupMenu(wxWindow *win, wxMenu *menu, int x, int y);

bool wxWindowOpenLayout::DoPopupMenu(wxMenu *menu, int x, int y)
{
    if ( x == wxDefaultCoord && y == wxDefaultCoord )
    {
        wxPoint p = ScreenToClient(wxGetMousePosition());
        x = p.x;
        y = p.y;
    }
    return wxOLPopupMenu((wxWindow *)this, menu, x, y);
}
#endif

#if wxUSE_TOOLTIPS
void wxWindowOpenLayout::DoSetToolTip(wxToolTip *tip)
{
    wxWindowBase::DoSetToolTip(tip);
    if ( tip )
        tip->SetWindow((wxWindow *)this);
    // the title attribute is in the markup
    OLRecreate();
}
#endif

#if wxUSE_DRAG_AND_DROP
void wxWindowOpenLayout::SetDropTarget(wxDropTarget *dropTarget)
{
    if ( m_dropTarget != dropTarget )
        delete m_dropTarget;
    m_dropTarget = dropTarget;
}

void wxWindowOpenLayout::DragAcceptFiles(bool WXUNUSED(accept))
{
}
#endif

// ---- the class's attributes ------------------------------------------------------------------

// wxWindowBase::GetClassDefaultAttributes: wx's common code (wincmn.cpp)

wxWindow *wxFindWindowAtPoint(const wxPoint& pt)
{
    return wxGenericFindWindowAtPoint(pt);
}

