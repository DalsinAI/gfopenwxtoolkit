/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/misc.cpp
// Purpose:     the port's shared pieces (the screen, cybergraphics, pens,
//              text), and wx's small system calls: the display, the
//              system's colours, fonts and sizes, the mouse and keys, the
//              busy pointer, the bell
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/window.h"
    #include "wx/toplevel.h"
    #include "wx/settings.h"
    #include "wx/font.h"
    #include "wx/cursor.h"
#endif

#include "wx/fontenum.h"
#include "wx/private/display.h"

#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/text.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>

#include <errno.h>
#include <string.h>

#include "private.h"

// workbench.library, opened only while a document is being opened
#define __NOLIBBASE__
#include <proto/wb.h>
#undef __NOLIBBASE__
static struct Library *WorkbenchBase;

// ---- cybergraphics ----------------------------------------------------------

struct Library *wxOLCyberGfxBase = NULL;

bool wxOLHaveCgx()
{
    static bool tried = false;
    if ( !tried )
    {
        tried = true;
        wxOLCyberGfxBase = OpenLibrary((STRPTR)"cybergraphics.library", 41);
    }
    return wxOLCyberGfxBase != NULL;
}

bool wxOLIsTrueColour(struct BitMap *bm)
{
    return bm && wxOLHaveCgx() && wxOL_CGX_MAPATTR(bm, wxOL_CYBRMATTR_ISCYBERGFX) &&
           wxOL_CGX_MAPATTR(bm, wxOL_CYBRMATTR_DEPTH) > 8;
}

// ---- the screen ----------------------------------------------------------------

static struct Screen *gs_screen = NULL;

struct Screen *wxOLGetScreen()
{
    if ( !gs_screen )
        gs_screen = LockPubScreen(NULL);
    return gs_screen;
}

void wxOLReleaseScreen()
{
    if ( gs_screen )
    {
        wxOLReleasePens();
        UnlockPubScreen(NULL, gs_screen);
        gs_screen = NULL;
    }
}

// ---- pens -----------------------------------------------------------------------

namespace
{
struct PenSlot { unsigned long rgb; long pen; };
wxVector<PenSlot> gs_pens;
}

long wxOLPenFor(unsigned long rgb)
{
    rgb &= 0xffffff;
    for ( size_t i = 0; i < gs_pens.size(); i++ )
        if ( gs_pens[i].rgb == rgb )
            return gs_pens[i].pen;
    struct Screen *scr = wxOLGetScreen();
    if ( !scr )
        return 1;
    ULONG r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
    LONG pen = ObtainBestPen(scr->ViewPort.ColorMap, r * 0x01010101UL, g * 0x01010101UL, b * 0x01010101UL,
                             OBP_Precision, PRECISION_IMAGE, TAG_DONE);
    if ( pen < 0 )
        pen = 1;
    PenSlot s = { rgb, pen };
    gs_pens.push_back(s);
    return pen;
}

void wxOLReleasePens()
{
    if ( gs_screen )
        for ( size_t i = 0; i < gs_pens.size(); i++ )
            ReleasePen(gs_screen->ViewPort.ColorMap, gs_pens[i].pen);
    gs_pens.clear();
}

// ---- text ------------------------------------------------------------------------

std::string wxOLToUTF8(const wxString& s)
{
    const wxScopedCharBuffer b = s.utf8_str();
    return std::string(b.data(), b.length());
}

wxString wxOLFromUTF8(const char *s)
{
    return s ? wxString::FromUTF8(s) : wxString();
}

std::string wxOLToLatin1(const wxString& s)
{
    std::string out;
    out.reserve(s.length());
    for ( wxString::const_iterator i = s.begin(); i != s.end(); ++i )
    {
        wxUniChar c = *i;
        out += c.GetValue() < 0x100 ? (char)c.GetValue() : '?';
    }
    return out;
}

wxString wxOLEscape(const wxString& s)
{
    wxString out;
    out.reserve(s.length());
    for ( wxString::const_iterator i = s.begin(); i != s.end(); ++i )
    {
        switch ( (*i).GetValue() )
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += *i; break;
        }
    }
    return out;
}

wxString wxOLStripMnemonics(const wxString& s)
{
    wxString out;
    for ( wxString::const_iterator i = s.begin(); i != s.end(); ++i )
    {
        if ( *i == '&' )
        {
            wxString::const_iterator n = i + 1;
            if ( n != s.end() && *n == '&' )
            {
                out += '&';
                i = n;
            }
            continue;
        }
        out += *i;
    }
    return out;
}

struct TextFont *wxOLScreenFont()
{
    static struct TextFont *tf = NULL;
    if ( !tf )
    {
        struct Screen *scr = wxOLGetScreen();
        if ( scr && scr->Font )
            tf = OpenFont(scr->Font);
        if ( !tf )
            tf = GfxBase->DefaultFont;
    }
    return tf;
}

void wxOLTextExtent(struct TextFont *tf, unsigned style, const wxString& s, int *w, int *h, int *descent)
{
    if ( !tf )
        tf = wxOLScreenFont();
    struct RastPort rp;
    InitRastPort(&rp);
    SetFont(&rp, tf);
    if ( style )
        SetSoftStyle(&rp, style, AskSoftStyle(&rp));
    // the widest line, the lines' height
    int lines = 1, widest = 0;
    size_t start = 0;
    const std::string t = wxOLToLatin1(s);
    for ( size_t i = 0; i <= t.length(); i++ )
    {
        if ( i == t.length() || t[i] == '\n' )
        {
            int lw = i > start ? TextLength(&rp, (STRPTR)t.data() + start, (UWORD)(i - start)) : 0;
            if ( lw > widest )
                widest = lw;
            if ( i < t.length() )
                lines++;
            start = i + 1;
        }
    }
    if ( w ) *w = widest;
    if ( h ) *h = tf->tf_YSize * lines;
    if ( descent ) *descent = tf->tf_YSize - tf->tf_Baseline - 1;
}

// ---- the display ---------------------------------------------------------------

namespace
{
class wxDisplayImplOpenLayout : public wxDisplayImplSingle
{
public:
    virtual wxRect GetGeometry() const wxOVERRIDE
    {
        struct Screen *scr = wxOLGetScreen();
        return scr ? wxRect(0, 0, scr->Width, scr->Height) : wxRect(0, 0, 640, 480);
    }
    virtual wxRect GetClientArea() const wxOVERRIDE
    {
        // below the screen's title bar
        struct Screen *scr = wxOLGetScreen();
        wxRect r = GetGeometry();
        if ( scr )
        {
            int t = scr->BarHeight + 1;
            r.y += t;
            r.height -= t;
        }
        return r;
    }
    virtual int GetDepth() const wxOVERRIDE
    {
        struct Screen *scr = wxOLGetScreen();
        return scr ? (int)GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) : 8;
    }
    virtual wxSize GetPPI() const wxOVERRIDE { return wxSize(72, 72); }
};

class wxDisplayFactoryOpenLayout : public wxDisplayFactorySingle
{
protected:
    virtual wxDisplayImpl *CreateSingleDisplay() wxOVERRIDE { return new wxDisplayImplOpenLayout; }
};
} // anonymous namespace

wxDisplayFactory *wxDisplay::CreateFactory()
{
    return new wxDisplayFactoryOpenLayout;
}

// ---- the system's colours, fonts and sizes --------------------------------------

wxColour wxSystemSettingsNative::GetColour(wxSystemColour index)
{
    // GFOpenGadTools' light theme's
    switch ( index )
    {
        case wxSYS_COLOUR_WINDOW:
        case wxSYS_COLOUR_LISTBOX:
        case wxSYS_COLOUR_INFOBK:
            return wxColour(0xff, 0xff, 0xff);
        case wxSYS_COLOUR_WINDOWTEXT:
        case wxSYS_COLOUR_BTNTEXT:
        case wxSYS_COLOUR_MENUTEXT:
        case wxSYS_COLOUR_CAPTIONTEXT:
        case wxSYS_COLOUR_INFOTEXT:
        case wxSYS_COLOUR_LISTBOXTEXT:
            return wxColour(0x00, 0x00, 0x00);
        case wxSYS_COLOUR_HIGHLIGHT:
        case wxSYS_COLOUR_ACTIVECAPTION:
        case wxSYS_COLOUR_HOTLIGHT:
        case wxSYS_COLOUR_MENUHILIGHT:
            return wxColour(0x3d, 0x6e, 0xb4);
        case wxSYS_COLOUR_HIGHLIGHTTEXT:
        case wxSYS_COLOUR_LISTBOXHIGHLIGHTTEXT:
            return wxColour(0xff, 0xff, 0xff);
        case wxSYS_COLOUR_GRAYTEXT:
        case wxSYS_COLOUR_INACTIVECAPTIONTEXT:
            return wxColour(0x80, 0x80, 0x80);
        case wxSYS_COLOUR_BTNSHADOW:
        case wxSYS_COLOUR_3DDKSHADOW:
            return wxColour(0x60, 0x60, 0x60);
        case wxSYS_COLOUR_BTNHIGHLIGHT:
        case wxSYS_COLOUR_3DLIGHT:
            return wxColour(0xf4, 0xf4, 0xf4);
        case wxSYS_COLOUR_WINDOWFRAME:
        case wxSYS_COLOUR_ACTIVEBORDER:
        case wxSYS_COLOUR_INACTIVEBORDER:
            return wxColour(0x80, 0x80, 0x80);
        default:
            // the face of buttons and windows, menus, the desktop
            return wxColour(0xd8, 0xd8, 0xd8);
    }
}

wxFont wxSystemSettingsNative::GetFont(wxSystemFont index)
{
    struct TextFont *tf = wxOLScreenFont();
    int px = tf ? tf->tf_YSize : 8;
    wxFontFamily family = wxFONTFAMILY_SWISS;
    wxString face;
    if ( index == wxSYS_OEM_FIXED_FONT || index == wxSYS_ANSI_FIXED_FONT || index == wxSYS_SYSTEM_FIXED_FONT )
    {
        family = wxFONTFAMILY_TELETYPE;
        face = "topaz";
    }
    else if ( tf && tf->tf_Message.mn_Node.ln_Name )
    {
        face = wxString(tf->tf_Message.mn_Node.ln_Name);
        face.Replace(".font", "");
    }
    // at the port's 72 pixels an inch, points are pixels
    return wxFont(px, family, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, face);
}

int wxSystemSettingsNative::GetMetric(wxSystemMetric index, const wxWindow *WXUNUSED(win))
{
    struct Screen *scr = wxOLGetScreen();
    switch ( index )
    {
        case wxSYS_SCREEN_X: return scr ? scr->Width : 640;
        case wxSYS_SCREEN_Y: return scr ? scr->Height : 480;
        case wxSYS_BORDER_X: case wxSYS_BORDER_Y: return 1;
        case wxSYS_EDGE_X: case wxSYS_EDGE_Y: return 2;
        case wxSYS_FRAMESIZE_X: return scr ? scr->WBorLeft : 4;
        case wxSYS_FRAMESIZE_Y: return scr ? scr->WBorBottom : 4;
        case wxSYS_CAPTION_Y: return scr ? scr->WBorTop + (scr->Font ? scr->Font->ta_YSize : 8) + 1 : 12;
        case wxSYS_MENU_Y: return scr ? scr->BarHeight + 1 : 11;
        case wxSYS_HSCROLL_Y: case wxSYS_VSCROLL_X: return 16;
        case wxSYS_VSCROLL_ARROW_X: case wxSYS_VSCROLL_ARROW_Y:
        case wxSYS_HSCROLL_ARROW_X: case wxSYS_HSCROLL_ARROW_Y: return 16;
        case wxSYS_VTHUMB_Y: case wxSYS_HTHUMB_X: return 16;
        case wxSYS_CURSOR_X: case wxSYS_CURSOR_Y: return 16;
        case wxSYS_ICON_X: case wxSYS_ICON_Y: return 32;
        case wxSYS_SMALLICON_X: case wxSYS_SMALLICON_Y: return 16;
        case wxSYS_DCLICK_X: case wxSYS_DCLICK_Y: return 4;
        case wxSYS_DRAG_X: case wxSYS_DRAG_Y: return 3;
        case wxSYS_DCLICK_MSEC: return 500;
        case wxSYS_CARET_ON_MSEC: case wxSYS_CARET_OFF_MSEC: return 500;
        case wxSYS_CARET_TIMEOUT_MSEC: return 0;
        case wxSYS_MOUSE_BUTTONS: return 3;
        case wxSYS_SWAP_BUTTONS: return 0;
        case wxSYS_NETWORK_PRESENT: return 1;
        default: return -1;
    }
}

// ---- the mouse, the keys ----------------------------------------------------------

void wxGetMousePosition(int *x, int *y)
{
    struct Screen *scr = wxOLGetScreen();
    if ( x ) *x = scr ? scr->MouseX : 0;
    if ( y ) *y = scr ? scr->MouseY : 0;
}

wxPoint wxGetMousePosition()
{
    int x, y;
    wxGetMousePosition(&x, &y);
    return wxPoint(x, y);
}

// the window under the pointer (wx's context help asks for it)
wxWindow *wxFindWindowAtPointer(wxPoint& pt)
{
    pt = wxGetMousePosition();
    return wxFindWindowAtPoint(pt);
}

// the qualifiers of the last input event the windows had (the event loop
// keeps them), for key state and mouse state
unsigned short wxOLLastQualifier = 0;

wxMouseState wxGetMouseState()
{
    wxMouseState ms;
    int x, y;
    wxGetMousePosition(&x, &y);
    ms.SetX(x);
    ms.SetY(y);
    ms.SetLeftDown((wxOLLastQualifier & IEQUALIFIER_LEFTBUTTON) != 0);
    ms.SetRightDown((wxOLLastQualifier & IEQUALIFIER_RBUTTON) != 0);
    ms.SetMiddleDown((wxOLLastQualifier & IEQUALIFIER_MIDBUTTON) != 0);
    ms.SetShiftDown((wxOLLastQualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0);
    ms.SetControlDown((wxOLLastQualifier & IEQUALIFIER_CONTROL) != 0);
    ms.SetAltDown((wxOLLastQualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) != 0);
    ms.SetMetaDown((wxOLLastQualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) != 0);
    return ms;
}

bool wxGetKeyState(wxKeyCode key)
{
    switch ( (int)key )
    {
        case WXK_SHIFT: return (wxOLLastQualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
        case WXK_CONTROL: return (wxOLLastQualifier & IEQUALIFIER_CONTROL) != 0;
        case WXK_ALT: return (wxOLLastQualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) != 0;
        case WXK_WINDOWS_LEFT: case WXK_WINDOWS_RIGHT:
            return (wxOLLastQualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) != 0;
        case WXK_CAPITAL: return (wxOLLastQualifier & IEQUALIFIER_CAPSLOCK) != 0;
        default: return false;
    }
}

// ---- the busy pointer, the bell ----------------------------------------------------

static int gs_busy = 0;

static void SetBusyPointers(bool busy)
{
    for ( wxWindowList::compatibility_iterator n = wxTopLevelWindows.GetFirst(); n; n = n->GetNext() )
    {
        wxNonOwnedWindow *tlw = wxDynamicCast(n->GetData(), wxNonOwnedWindow);
        struct Window *w = tlw ? tlw->OLGetWindow() : NULL;
        if ( w )
            SetWindowPointer(w, WA_BusyPointer, busy, WA_PointerDelay, busy, TAG_DONE);
    }
}

void wxBeginBusyCursor(const wxCursor *WXUNUSED(cursor))
{
    if ( gs_busy++ == 0 )
        SetBusyPointers(true);
}

void wxEndBusyCursor()
{
    if ( gs_busy > 0 && --gs_busy == 0 )
        SetBusyPointers(false);
}

bool wxIsBusy()
{
    return gs_busy > 0;
}

void wxBell()
{
    DisplayBeep(wxOLGetScreen());
}

wxWindow *wxGetActiveWindow()
{
    for ( wxWindowList::compatibility_iterator n = wxTopLevelWindows.GetFirst(); n; n = n->GetNext() )
    {
        wxNonOwnedWindow *tlw = wxDynamicCast(n->GetData(), wxNonOwnedWindow);
        struct Window *w = tlw ? tlw->OLGetWindow() : NULL;
        if ( w && (w->Flags & WFLG_WINDOWACTIVE) )
            return tlw;
    }
    return NULL;
}

// ---- fonts -------------------------------------------------------------------------

bool wxFontEnumerator::EnumerateFacenames(wxFontEncoding WXUNUSED(encoding), bool fixedWidthOnly)
{
    // the fonts in FONTS:, through diskfont.library
    LONG size = 4096;
    for ( int tries = 0; tries < 4; tries++, size *= 2 )
    {
        struct AvailFontsHeader *afh = (struct AvailFontsHeader *)AllocVec(size, MEMF_ANY | MEMF_CLEAR);
        if ( !afh )
            return false;
        LONG more = AvailFonts(afh, size, AFF_DISK | AFF_MEMORY);
        if ( more > 0 )
        {
            FreeVec(afh);
            continue;
        }
        wxArrayString seen;
        struct AvailFonts *af = (struct AvailFonts *)(afh + 1);
        bool go = true;
        for ( UWORD i = 0; i < afh->afh_NumEntries && go; i++ )
        {
            wxString name(af[i].af_Attr.ta_Name);
            name.Replace(".font", "");
            if ( fixedWidthOnly && (af[i].af_Attr.ta_Flags & FPF_PROPORTIONAL) )
                continue;
            if ( seen.Index(name) != wxNOT_FOUND )
                continue;
            seen.Add(name);
            go = OnFacename(name);
        }
        FreeVec(afh);
        return true;
    }
    return false;
}

bool wxFontEnumerator::EnumerateEncodings(const wxString& facename)
{
    // the Amiga's fonts are Latin-1
    return OnFontEncoding(facename, "iso8859-1");
}

// ----------------------------------------------------------------------------
// wxLaunchDefaultApplication: Workbench opens the document the way a double
// click on its icon does (its default tool, or the file type's), with
// workbench.library 44's OpenWorkbenchObjectA. False when Workbench isn't
// running or can't open it.
// ----------------------------------------------------------------------------

bool wxLaunchDefaultApplication(const wxString& document, int WXUNUSED(flags))
{
    struct Library *base = OpenLibrary((CONST_STRPTR)"workbench.library", 44);
    if ( !base )
        return false;
    WorkbenchBase = base;
    const wxCharBuffer name = document.fn_str();
    const BOOL ok = OpenWorkbenchObjectA((CONST_STRPTR)name.data(), NULL);
    WorkbenchBase = NULL;
    CloseLibrary(base);
    return ok != FALSE;
}
