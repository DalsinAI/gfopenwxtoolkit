/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/dc.cpp
// Purpose:     wxOLDCImpl: wx's drawing through graphics.library into a
//              RastPort, filled boxes in exact colours through cybergraphics
//              on a true-colour screen; and the window, client, paint,
//              memory and screen DCs
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/dcmemory.h"
    #include "wx/dcclient.h"
    #include "wx/dcscreen.h"
    #include "wx/icon.h"
    #include "wx/image.h"
    #include "wx/math.h"
#endif

#include "wx/nonownedwnd.h"
#include "wx/openlayout/dc.h"
#include "wx/openlayout/dcclient.h"
#include "wx/openlayout/dcmemory.h"
#include "wx/openlayout/dcscreen.h"

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>

#include <stdlib.h>
#include <string.h>

#include "private.h"

void wxOLBitmapDrawnOn(wxBitmap& bmp);

// the paint the TLW is giving a window now (window.cpp sets it around the
// wxPaintEvent): where olui wants the window's picture
struct wxOLPaintContext
{
    wxWindow *win;
    struct RastPort *rp;
    int x, y, w, h;
};
extern wxOLPaintContext *wxOLCurrentPaint;

namespace
{
inline unsigned long ARGB(const wxColour& c)
{
    return 0xff000000UL | ((unsigned long)c.Red() << 16) | ((unsigned long)c.Green() << 8) | c.Blue();
}

// a span list for filled shapes: x0..x1 on row y, each row's spans drawn
// as one box
struct Span { int y, x0, x1; };
}

wxIMPLEMENT_ABSTRACT_CLASS(wxOLDCImpl, wxDCImpl);

wxOLDCImpl::wxOLDCImpl(wxDC *owner)
    : wxDCImpl(owner), m_rp(NULL), m_ox(0), m_oy(0), m_w(0), m_h(0), m_olClipped(false), m_olRegion(NULL),
      m_olOldRegion(NULL), m_olDirect(false), m_olPens(NULL)
{
    m_pen = *wxBLACK_PEN;
    m_brush = *wxWHITE_BRUSH;
    m_backgroundBrush = *wxWHITE_BRUSH;
    m_font = *wxNORMAL_FONT;
    m_textForegroundColour = *wxBLACK;
    m_textBackgroundColour = *wxWHITE;
}

wxOLDCImpl::~wxOLDCImpl()
{
    OLRemoveClip();
}

void wxOLDCImpl::OLInit(struct RastPort *rp, int ox, int oy, int w, int h)
{
    OLRemoveClip();
    m_rp = rp;
    m_ox = ox;
    m_oy = oy;
    m_w = w;
    m_h = h;
    m_ok = rp != NULL;
    m_olDirect = rp && wxOLIsTrueColour(rp->BitMap);
    m_olClip = wxRect(0, 0, w, h);
    m_olClipped = false;
    OLApplyClip();
}

// ---- clipping: an Amiga region in the layer, our box cut to the user's
// clip and to what was there (olui's, while it draws) --------------------

void wxOLDCImpl::OLApplyClip()
{
    if ( !m_rp || !m_rp->Layer )
        return;
    struct Region *r = NewRegion();
    if ( !r )
        return;
    struct Rectangle rect;
    rect.MinX = (WORD)(m_ox + m_olClip.x);
    rect.MinY = (WORD)(m_oy + m_olClip.y);
    rect.MaxX = (WORD)(m_ox + m_olClip.x + m_olClip.width - 1);
    rect.MaxY = (WORD)(m_oy + m_olClip.y + m_olClip.height - 1);
    if ( m_olClip.width > 0 && m_olClip.height > 0 )
        OrRectRegion(r, &rect);
    struct Layer *l = m_rp->Layer;
    struct Region *was = m_olRegion ? m_olOldRegion : l->ClipRegion;
    if ( was )
        AndRegionRegion(was, r);
    LockLayerInfo(l->LayerInfo);
    struct Region *old = InstallClipRegion(l, r);
    UnlockLayerInfo(l->LayerInfo);
    if ( !m_olRegion )
        m_olOldRegion = old;
    else
        DisposeRegion(m_olRegion);
    m_olRegion = r;
}

void wxOLDCImpl::OLRemoveClip()
{
    if ( m_olRegion && m_rp && m_rp->Layer )
    {
        struct Layer *l = m_rp->Layer;
        LockLayerInfo(l->LayerInfo);
        InstallClipRegion(l, m_olOldRegion);
        UnlockLayerInfo(l->LayerInfo);
        DisposeRegion(m_olRegion);
    }
    m_olRegion = NULL;
    m_olOldRegion = NULL;
}

void wxOLDCImpl::DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    wxDCImpl::DoSetClippingRegion(x, y, width, height);
    wxRect r(LogicalToDevice(x, y), LogicalToDeviceRel(width, height));
    if ( r.width < 0 ) { r.x += r.width; r.width = -r.width; }
    if ( r.height < 0 ) { r.y += r.height; r.height = -r.height; }
    r.Intersect(m_olClipped ? m_olClip : wxRect(0, 0, m_w, m_h));
    m_olClip = r;
    m_olClipped = true;
    OLApplyClip();
}

void wxOLDCImpl::DoSetDeviceClippingRegion(const wxRegion& region)
{
    wxRect box = region.GetBox();
    if ( m_olClipped )
        box.Intersect(m_olClip);
    else
        box.Intersect(wxRect(0, 0, m_w, m_h));
    // wx's clipping box from it, in logical coordinates
    wxDCImpl::DoSetClippingRegion(DeviceToLogicalX(box.x), DeviceToLogicalY(box.y),
                                  DeviceToLogicalXRel(box.width), DeviceToLogicalYRel(box.height));
    m_olClip = box;
    m_olClipped = true;
    OLApplyClip();
}

void wxOLDCImpl::DestroyClippingRegion()
{
    wxDCImpl::DestroyClippingRegion();
    m_olClip = wxRect(0, 0, m_w, m_h);
    m_olClipped = false;
    OLApplyClip();
}

bool wxOLDCImpl::OLCut(int& x, int& y, int& w, int& h) const
{
    wxRect r(x, y, w, h);
    r.Intersect(m_olClip);
    if ( r.IsEmpty() )
        return false;
    x = r.x; y = r.y; w = r.width; h = r.height;
    return true;
}

// ---- colours -----------------------------------------------------------------------

void wxOLDCImpl::OLSetAPen(const wxColour& c)
{
    SetAPen(m_rp, (ULONG)wxOLPenFor(ARGB(c)));
    switch ( m_logicalFunction )
    {
        case wxINVERT:
        case wxXOR:
        case wxEQUIV:
        case wxSRC_INVERT:
            SetDrMd(m_rp, COMPLEMENT);
            break;
        default:
            SetDrMd(m_rp, JAM1);
            break;
    }
}

// a device box in a colour (cut to the clip by the layer)
void wxOLDCImpl::OLFill(int x, int y, int w, int h, const wxColour& c)
{
    if ( !OLCut(x, y, w, h) )
        return;
    if ( m_olDirect && m_logicalFunction == wxCOPY )
    {
        wxOL_CGX_FILL(m_rp, m_ox + x, m_oy + y, w, h, ARGB(c));
        return;
    }
    OLSetAPen(c);
    RectFill(m_rp, m_ox + x, m_oy + y, m_ox + x + w - 1, m_oy + y + h - 1);
    SetDrMd(m_rp, JAM1);
}

struct TextFont *wxOLDCImpl::OLFont() const
{
    struct TextFont *tf = m_font.IsOk() ? m_font.GetTextFont() : NULL;
    return tf ? tf : wxOLScreenFont();
}

// ---- what wx asks -------------------------------------------------------------------

void wxOLDCImpl::Clear()
{
    if ( !m_rp )
        return;
    wxColour c = m_backgroundBrush.IsOk() ? m_backgroundBrush.GetColour() : *wxWHITE;
    wxRasterOperationMode f = m_logicalFunction;
    m_logicalFunction = wxCOPY;
    OLFill(0, 0, m_w, m_h, c);
    m_logicalFunction = f;
}

void wxOLDCImpl::SetFont(const wxFont& font) { m_font = font; }
void wxOLDCImpl::SetPen(const wxPen& pen) { m_pen = pen; }
void wxOLDCImpl::SetBrush(const wxBrush& brush) { m_brush = brush; }
void wxOLDCImpl::SetBackground(const wxBrush& brush) { m_backgroundBrush = brush; }
void wxOLDCImpl::SetBackgroundMode(int mode) { m_backgroundMode = mode; }
void wxOLDCImpl::SetLogicalFunction(wxRasterOperationMode function) { m_logicalFunction = function; }

wxCoord wxOLDCImpl::GetCharHeight() const
{
    return OLFont()->tf_YSize;
}

wxCoord wxOLDCImpl::GetCharWidth() const
{
    struct TextFont *tf = OLFont();
    return tf->tf_XSize;
}

static unsigned SoftStyleFor(const wxFont& font, struct TextFont *tf)
{
    unsigned want = 0;
    if ( font.IsOk() )
    {
        if ( font.GetUnderlined() )
            want |= FSF_UNDERLINED;
        if ( font.GetNumericWeight() >= wxFONTWEIGHT_BOLD )
            want |= FSF_BOLD;
        if ( font.GetStyle() != wxFONTSTYLE_NORMAL )
            want |= FSF_ITALIC;
    }
    // what the font doesn't have of itself
    return want & ~(unsigned)tf->tf_Style;
}

void wxOLDCImpl::DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y, wxCoord *descent,
                                 wxCoord *externalLeading, const wxFont *theFont) const
{
    const wxFont& f = theFont && theFont->IsOk() ? *theFont : m_font;
    struct TextFont *tf = f.IsOk() ? f.GetTextFont() : NULL;
    if ( !tf )
        tf = wxOLScreenFont();
    int w, h, d;
    wxOLTextExtent(tf, SoftStyleFor(f, tf), string, &w, &h, &d);
    if ( x ) *x = w;
    if ( y ) *y = h;
    if ( descent ) *descent = d;
    if ( externalLeading ) *externalLeading = 0;
}

bool wxOLDCImpl::DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const
{
    struct TextFont *tf = OLFont();
    struct RastPort rp;
    InitRastPort(&rp);
    ::SetFont(&rp, tf);
    unsigned st = SoftStyleFor(m_font, tf);
    if ( st )
        SetSoftStyle(&rp, st, AskSoftStyle(&rp));
    const std::string t = wxOLToLatin1(text);
    widths.Empty();
    widths.Alloc(t.length());
    for ( size_t i = 1; i <= t.length(); i++ )
        widths.Add(TextLength(&rp, (STRPTR)t.data(), (UWORD)i));
    return true;
}

int wxOLDCImpl::GetDepth() const
{
    return m_rp ? (int)GetBitMapAttr(m_rp->BitMap, BMA_DEPTH) : 24;
}

wxSize wxOLDCImpl::GetPPI() const
{
    return wxSize(72, 72);
}

void wxOLDCImpl::DoGetSize(int *width, int *height) const
{
    if ( width ) *width = m_w;
    if ( height ) *height = m_h;
}

void wxOLDCImpl::DoGetSizeMM(int *width, int *height) const
{
    if ( width ) *width = m_w * 254 / 720;
    if ( height ) *height = m_h * 254 / 720;
}

// ---- pixels ---------------------------------------------------------------------------

bool wxOLDCImpl::DoFloodFill(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y), const wxColour& WXUNUSED(col),
                             wxFloodFillStyle WXUNUSED(style))
{
    return false;
}

bool wxOLDCImpl::DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const
{
    if ( !m_rp || !col )
        return false;
    int dx = LogicalToDeviceX(x), dy = LogicalToDeviceY(y);
    if ( dx < 0 || dy < 0 || dx >= m_w || dy >= m_h )
        return false;
    if ( m_olDirect )
    {
        ULONG p = 0;
        wxOL_CGX_READ(&p, 0, 0, 4, m_rp, m_ox + dx, m_oy + dy, 1, 1);
        col->Set((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff);
        return true;
    }
    LONG pen = ReadPixel(m_rp, m_ox + dx, m_oy + dy);
    struct Screen *scr = wxOLGetScreen();
    if ( pen < 0 || !scr )
        return false;
    ULONG rgb[3];
    GetRGB32(scr->ViewPort.ColorMap, (ULONG)pen, 1, rgb);
    col->Set(rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
    return true;
}

// ---- lines -------------------------------------------------------------------------------

static bool PenDraws(const wxPen& pen)
{
    return pen.IsOk() && pen.GetStyle() != wxPENSTYLE_TRANSPARENT;
}

static UWORD PatternFor(const wxPen& pen)
{
    switch ( pen.GetStyle() )
    {
        case wxPENSTYLE_DOT: return 0xaaaa;
        case wxPENSTYLE_LONG_DASH: return 0xfff0;
        case wxPENSTYLE_SHORT_DASH: return 0xf0f0;
        case wxPENSTYLE_DOT_DASH: return 0xfe38;
        case wxPENSTYLE_USER_DASH:
        {
            // the dashes as a 16-bit pattern, a bit a pixel
            wxDash *d;
            int n = pen.GetDashes(&d);
            if ( n <= 0 || !d )
                return 0xffff;
            UWORD pat = 0;
            int bit = 15, i = 0;
            bool on = true;
            while ( bit >= 0 )
            {
                int len = d[i % n] > 0 ? d[i % n] : 1;
                for ( int k = 0; k < len && bit >= 0; k++, bit-- )
                    if ( on )
                        pat |= (UWORD)(1u << bit);
                on = !on;
                i++;
            }
            return pat;
        }
        default: return 0xffff;
    }
}

void wxOLDCImpl::DoDrawPoint(wxCoord x, wxCoord y)
{
    if ( !m_rp || !PenDraws(m_pen) )
        return;
    OLFill(LogicalToDeviceX(x), LogicalToDeviceY(y), 1, 1, m_pen.GetColour());
    CalcBoundingBox(x, y);
}

void wxOLDCImpl::DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2)
{
    if ( !m_rp || !PenDraws(m_pen) )
        return;
    int dx1 = LogicalToDeviceX(x1), dy1 = LogicalToDeviceY(y1);
    int dx2 = LogicalToDeviceX(x2), dy2 = LogicalToDeviceY(y2);
    int width = m_pen.GetWidth() > 1 ? LogicalToDeviceXRel(m_pen.GetWidth()) : 1;
    if ( width < 1 )
        width = 1;
    const UWORD pat = PatternFor(m_pen);
    // straight solid lines are boxes (cybergraphics' exact colour)
    if ( pat == 0xffff && (dy1 == dy2 || dx1 == dx2) )
    {
        int half = (width - 1) / 2;
        if ( dy1 == dy2 )
        {
            int a = wxMin(dx1, dx2), b = wxMax(dx1, dx2);
            // wx leaves the last point out
            OLFill(a, dy1 - half, b - a + (dx1 == dx2 ? 1 : 0), width, m_pen.GetColour());
        }
        else
        {
            int a = wxMin(dy1, dy2), b = wxMax(dy1, dy2);
            OLFill(dx1 - half, a, width, b - a, m_pen.GetColour());
        }
        CalcBoundingBox(x1, y1);
        CalcBoundingBox(x2, y2);
        return;
    }
    OLSetAPen(m_pen.GetColour());
    SetDrPt(m_rp, pat);
    if ( width == 1 )
    {
        Move(m_rp, m_ox + dx1, m_oy + dy1);
        Draw(m_rp, m_ox + dx2, m_oy + dy2);
    }
    else
    {
        // a wide line: lines side by side across it
        int half = width / 2;
        bool steep = abs(dy2 - dy1) > abs(dx2 - dx1);
        for ( int o = -half; o < width - half; o++ )
        {
            int ox = steep ? o : 0, oy = steep ? 0 : o;
            Move(m_rp, m_ox + dx1 + ox, m_oy + dy1 + oy);
            Draw(m_rp, m_ox + dx2 + ox, m_oy + dy2 + oy);
        }
    }
    SetDrPt(m_rp, 0xffff);
    SetDrMd(m_rp, JAM1);
    CalcBoundingBox(x1, y1);
    CalcBoundingBox(x2, y2);
}

void wxOLDCImpl::DoDrawLines(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset)
{
    for ( int i = 1; i < n; i++ )
        DoDrawLine(points[i - 1].x + xoffset, points[i - 1].y + yoffset, points[i].x + xoffset,
                   points[i].y + yoffset);
}

void wxOLDCImpl::DoCrossHair(wxCoord x, wxCoord y)
{
    if ( !m_rp || !PenDraws(m_pen) )
        return;
    int dx = LogicalToDeviceX(x), dy = LogicalToDeviceY(y);
    OLFill(0, dy, m_w, 1, m_pen.GetColour());
    OLFill(dx, 0, 1, m_h, m_pen.GetColour());
}

// ---- filled shapes: spans --------------------------------------------------------------

namespace
{
// a polygon's spans, even-odd or winding, in device coordinates
void PolygonSpans(int n, const wxPoint *p, bool winding, wxVector<Span>& out)
{
    if ( n < 3 )
        return;
    int ymin = p[0].y, ymax = p[0].y;
    for ( int i = 1; i < n; i++ )
    {
        ymin = wxMin(ymin, p[i].y);
        ymax = wxMax(ymax, p[i].y);
    }
    wxVector<int> xs;
    wxVector<int> dirs;
    for ( int y = ymin; y < ymax; y++ )
    {
        xs.clear();
        dirs.clear();
        // the edges crossing the row's middle
        for ( int i = 0; i < n; i++ )
        {
            const wxPoint& a = p[i];
            const wxPoint& b = p[(i + 1) % n];
            if ( a.y == b.y )
                continue;
            int y0 = wxMin(a.y, b.y), y1 = wxMax(a.y, b.y);
            if ( y < y0 || y >= y1 )
                continue;
            // x at y + 0.5, in fixed point
            long num = (long)(2 * y + 1 - 2 * a.y) * (b.x - a.x);
            int x = a.x + (int)(num / (2 * (b.y - a.y)));
            // sorted in
            size_t k = xs.size();
            xs.push_back(x);
            dirs.push_back(b.y > a.y ? 1 : -1);
            while ( k > 0 && xs[k - 1] > xs[k] )
            {
                wxSwap(xs[k - 1], xs[k]);
                wxSwap(dirs[k - 1], dirs[k]);
                k--;
            }
        }
        if ( !winding )
        {
            for ( size_t k = 0; k + 1 < xs.size(); k += 2 )
                if ( xs[k + 1] > xs[k] )
                {
                    Span s = { y, xs[k], xs[k + 1] - 1 };
                    out.push_back(s);
                }
        }
        else
        {
            int w = 0;
            for ( size_t k = 0; k + 1 < xs.size(); k++ )
            {
                w += dirs[k];
                if ( w != 0 && xs[k + 1] > xs[k] )
                {
                    Span s = { y, xs[k], xs[k + 1] - 1 };
                    out.push_back(s);
                }
            }
        }
    }
}

// an ellipse's spans (the box x, y, w, h), midpoint by rows
void EllipseSpans(int x, int y, int w, int h, wxVector<Span>& out)
{
    if ( w <= 0 || h <= 0 )
        return;
    // for each row, the half width at its middle: (dx/a)^2 + (dy/b)^2 <= 1
    // with a = w/2, b = h/2, in quarters of a pixel so it stays whole
    const long a2 = (long)w * w, b2 = (long)h * h;
    for ( int r = 0; r < h; r++ )
    {
        long dy = 2 * r + 1 - h;      // twice the row's middle from the centre
        long lhs = b2 - dy * dy;      // a^2 b^2 (1 - (dy/b)^2) / a^2 ... scaled
        if ( lhs <= 0 )
            continue;
        // half width (doubled) = w * sqrt(lhs) / h
        long hw2 = 0;
        long v = lhs * a2 / b2;
        // integer square root
        long s = 0, bit = 1L << 30;
        while ( bit > v ) bit >>= 2;
        while ( bit )
        {
            if ( v >= s + bit ) { v -= s + bit; s = (s >> 1) + bit; }
            else s >>= 1;
            bit >>= 2;
        }
        hw2 = s;
        int x0 = x + (int)((w - hw2 + 1) / 2);
        int x1 = x + (int)((w + hw2) / 2) - 1;
        if ( x1 >= x0 )
        {
            Span sp = { y + r, x0, x1 };
            out.push_back(sp);
        }
    }
}
} // anonymous namespace

// spans in a colour, or each span's ends (an outline) in another
static void FillSpans(wxOLDCImpl *dc, const wxVector<Span>& spans, const wxColour& c,
                      void (*fill)(wxOLDCImpl *, int, int, int, int, const wxColour&))
{
    for ( size_t i = 0; i < spans.size(); i++ )
        fill(dc, spans[i].x0, spans[i].y, spans[i].x1 - spans[i].x0 + 1, 1, c);
}

namespace
{
class FillAccess : public wxOLDCImpl
{
public:
    static void Fill(wxOLDCImpl *dc, int x, int y, int w, int h, const wxColour& c)
    {
        static_cast<FillAccess *>(dc)->OLFill(x, y, w, h, c);
    }
};
}

static bool BrushFills(const wxBrush& b)
{
    return b.IsOk() && b.GetStyle() != wxBRUSHSTYLE_TRANSPARENT;
}

void wxOLDCImpl::DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    if ( !m_rp )
        return;
    int dx = LogicalToDeviceX(x), dy = LogicalToDeviceY(y);
    int dw = LogicalToDeviceXRel(width), dh = LogicalToDeviceYRel(height);
    if ( dw < 0 ) { dx += dw; dw = -dw; }
    if ( dh < 0 ) { dy += dh; dh = -dh; }
    if ( BrushFills(m_brush) )
        OLFill(dx, dy, dw, dh, m_brush.GetColour());
    if ( PenDraws(m_pen) )
    {
        int pw = m_pen.GetWidth() > 1 ? m_pen.GetWidth() : 1;
        const wxColour c = m_pen.GetColour();
        if ( PatternFor(m_pen) == 0xffff )
        {
            OLFill(dx, dy, dw, pw, c);
            OLFill(dx, dy + dh - pw, dw, pw, c);
            OLFill(dx, dy, pw, dh, c);
            OLFill(dx + dw - pw, dy, pw, dh, c);
        }
        else
        {
            wxPoint pts[5] = { wxPoint(x, y), wxPoint(x + width - 1, y), wxPoint(x + width - 1, y + height - 1),
                               wxPoint(x, y + height - 1), wxPoint(x, y) };
            DoDrawLines(5, pts, 0, 0);
        }
    }
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + width, y + height);
}

void wxOLDCImpl::DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius)
{
    if ( radius < 0 )
        radius = -radius * wxMin(width, height);
    int r = (int)radius;
    if ( r <= 1 || !m_rp )
    {
        DoDrawRectangle(x, y, width, height);
        return;
    }
    // the corners cut along a quarter circle, as a polygon
    wxVector<wxPoint> pts;
    const int steps = 6;
    for ( int c = 0; c < 4; c++ )
    {
        int cx = c == 0 || c == 3 ? x + r : x + width - 1 - r;
        int cy = c < 2 ? y + r : y + height - 1 - r;
        for ( int i = 0; i <= steps; i++ )
        {
            double a = (c * 90 + 180 + 90.0 * i / steps) * M_PI / 180.0;
            pts.push_back(wxPoint(cx + (int)(r * cos(a)), cy + (int)(r * sin(a))));
        }
    }
    DoDrawPolygon((int)pts.size(), &pts[0], 0, 0, wxODDEVEN_RULE);
}

void wxOLDCImpl::DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height)
{
    if ( !m_rp )
        return;
    int dx = LogicalToDeviceX(x), dy = LogicalToDeviceY(y);
    int dw = LogicalToDeviceXRel(width), dh = LogicalToDeviceYRel(height);
    if ( BrushFills(m_brush) )
    {
        wxVector<Span> spans;
        EllipseSpans(dx, dy, dw, dh, spans);
        FillSpans(this, spans, m_brush.GetColour(), FillAccess::Fill);
    }
    if ( PenDraws(m_pen) )
    {
        // the outline: each row's ends, and the top and bottom rows whole
        wxVector<Span> spans;
        EllipseSpans(dx, dy, dw, dh, spans);
        const wxColour c = m_pen.GetColour();
        for ( size_t i = 0; i < spans.size(); i++ )
        {
            int prev0 = i > 0 ? spans[i - 1].x0 : spans[i].x1, prev1 = i > 0 ? spans[i - 1].x1 : spans[i].x0;
            int next0 = i + 1 < spans.size() ? spans[i + 1].x0 : spans[i].x1;
            int next1 = i + 1 < spans.size() ? spans[i + 1].x1 : spans[i].x0;
            int l = wxMin(prev0, next0), rr = wxMax(prev1, next1);
            // left edge from x0 to where the neighbours start
            int le = wxMax(spans[i].x0, l - 1);
            OLFill(spans[i].x0, spans[i].y, le - spans[i].x0 + 1, 1, c);
            int rs = wxMin(spans[i].x1, rr + 1);
            OLFill(rs, spans[i].y, spans[i].x1 - rs + 1, 1, c);
        }
    }
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + width, y + height);
}

void wxOLDCImpl::DoDrawPolygon(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle)
{
    if ( !m_rp || n < 2 )
        return;
    wxVector<wxPoint> dev(n);
    for ( int i = 0; i < n; i++ )
    {
        dev[i].x = LogicalToDeviceX(points[i].x + xoffset);
        dev[i].y = LogicalToDeviceY(points[i].y + yoffset);
        CalcBoundingBox(points[i].x + xoffset, points[i].y + yoffset);
    }
    if ( BrushFills(m_brush) )
    {
        wxVector<Span> spans;
        PolygonSpans(n, &dev[0], fillStyle == wxWINDING_RULE, spans);
        FillSpans(this, spans, m_brush.GetColour(), FillAccess::Fill);
    }
    if ( PenDraws(m_pen) )
    {
        OLSetAPen(m_pen.GetColour());
        SetDrPt(m_rp, PatternFor(m_pen));
        Move(m_rp, m_ox + dev[0].x, m_oy + dev[0].y);
        for ( int i = 1; i < n; i++ )
            Draw(m_rp, m_ox + dev[i].x, m_oy + dev[i].y);
        Draw(m_rp, m_ox + dev[0].x, m_oy + dev[0].y);
        SetDrPt(m_rp, 0xffff);
        SetDrMd(m_rp, JAM1);
    }
}

void wxOLDCImpl::DoDrawArc(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2, wxCoord xc, wxCoord yc)
{
    // from (x1, y1) to (x2, y2) anticlockwise round (xc, yc), as a pie
    double dx = x1 - xc, dy = y1 - yc;
    double r = sqrt(dx * dx + dy * dy);
    double a1 = atan2(-(double)(y1 - yc), (double)(x1 - xc)) * 180.0 / M_PI;
    double a2 = atan2(-(double)(y2 - yc), (double)(x2 - xc)) * 180.0 / M_PI;
    int ir = (int)r;
    DoDrawEllipticArc(xc - ir, yc - ir, 2 * ir, 2 * ir, a1, x1 == x2 && y1 == y2 ? a1 + 360 : a2);
}

void wxOLDCImpl::DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h, double sa, double ea)
{
    if ( !m_rp )
        return;
    while ( ea < sa )
        ea += 360;
    const double cx = x + w / 2.0, cy = y + h / 2.0, rx = w / 2.0, ry = h / 2.0;
    int steps = (int)((ea - sa) / 6) + 2;
    wxVector<wxPoint> pts;
    pts.push_back(wxPoint((int)cx, (int)cy));
    for ( int i = 0; i <= steps; i++ )
    {
        double a = (sa + (ea - sa) * i / steps) * M_PI / 180.0;
        pts.push_back(wxPoint((int)(cx + rx * cos(a)), (int)(cy - ry * sin(a))));
    }
    if ( BrushFills(m_brush) )
    {
        wxPen pen = m_pen;
        m_pen = *wxTRANSPARENT_PEN;
        DoDrawPolygon((int)pts.size(), &pts[0], 0, 0, wxODDEVEN_RULE);
        m_pen = pen;
    }
    if ( PenDraws(m_pen) )
        DoDrawLines((int)pts.size() - 1, &pts[1], 0, 0);
}

// ---- text --------------------------------------------------------------------------------

void wxOLDCImpl::DoDrawText(const wxString& text, wxCoord x, wxCoord y)
{
    if ( !m_rp || text.empty() )
        return;
    struct TextFont *tf = OLFont();
    ::SetFont(m_rp, tf);
    unsigned st = SoftStyleFor(m_font, tf);
    SetSoftStyle(m_rp, st, 0xff);
    SetAPen(m_rp, (ULONG)wxOLPenFor(ARGB(m_textForegroundColour)));
    if ( m_backgroundMode == wxBRUSHSTYLE_SOLID )
    {
        SetBPen(m_rp, (ULONG)wxOLPenFor(ARGB(m_textBackgroundColour)));
        SetDrMd(m_rp, JAM2);
    }
    else
        SetDrMd(m_rp, JAM1);
    if ( m_logicalFunction == wxINVERT || m_logicalFunction == wxXOR )
        SetDrMd(m_rp, COMPLEMENT);

    const std::string t = wxOLToLatin1(text);
    int dx = LogicalToDeviceX(x), dy = LogicalToDeviceY(y);
    int line = 0, widest = 0;
    size_t start = 0;
    for ( size_t i = 0; i <= t.length(); i++ )
    {
        if ( i < t.length() && t[i] != '\n' )
            continue;
        if ( i > start )
        {
            Move(m_rp, m_ox + dx, m_oy + dy + line * tf->tf_YSize + tf->tf_Baseline);
            Text(m_rp, (STRPTR)t.data() + start, (UWORD)(i - start));
            int lw = TextLength(m_rp, (STRPTR)t.data() + start, (UWORD)(i - start));
            widest = wxMax(widest, lw);
        }
        line++;
        start = i + 1;
    }
    SetSoftStyle(m_rp, 0, 0xff);
    SetDrMd(m_rp, JAM1);
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + DeviceToLogicalXRel(widest), y + DeviceToLogicalYRel(line * tf->tf_YSize));
}

void wxOLDCImpl::DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle)
{
    if ( angle == 0.0 )
    {
        DoDrawText(text, x, y);
        return;
    }
#if wxUSE_IMAGE
    // drawn level into a bitmap, turned, and drawn with the text's pixels only
    wxCoord w, h;
    DoGetTextExtent(text, &w, &h);
    if ( w <= 0 || h <= 0 )
        return;
    wxBitmap bmp(w, h, 24);
    {
        wxMemoryDC mdc(bmp);
        mdc.SetBackground(*wxBLACK_BRUSH);
        mdc.Clear();
        mdc.SetFont(m_font);
        mdc.SetTextForeground(*wxWHITE);
        mdc.DrawText(text, 0, 0);
    }
    wxImage img = bmp.ConvertToImage();
    img.SetAlpha();
    unsigned char *rgb = img.GetData(), *a = img.GetAlpha();
    for ( int i = 0, n = w * h; i < n; i++ )
    {
        a[i] = rgb[i * 3];
        rgb[i * 3] = m_textForegroundColour.Red();
        rgb[i * 3 + 1] = m_textForegroundColour.Green();
        rgb[i * 3 + 2] = m_textForegroundColour.Blue();
    }
    wxPoint centre;
    wxImage turned = img.Rotate(angle * M_PI / 180.0, wxPoint(0, 0), true, &centre);
    // where the text's top left went
    double rad = angle * M_PI / 180.0;
    int ox = 0, oy = 0;
    double c = cos(rad), s = sin(rad);
    double xs[4] = { 0, w * c, w * c + h * s, h * s };
    double ys[4] = { 0, -w * s, -w * s + h * c, h * c };
    double minx = 0, miny = 0;
    for ( int i = 0; i < 4; i++ )
    {
        minx = wxMin(minx, xs[i]);
        miny = wxMin(miny, ys[i]);
    }
    ox = (int)minx;
    oy = (int)miny;
    DoDrawBitmap(wxBitmap(turned), x + ox, y + oy, true);
#else
    DoDrawText(text, x, y);
#endif
}

// ---- bitmaps -------------------------------------------------------------------------------

void wxOLDCImpl::OLPutBitmap(const wxBitmap& bmp, int sx, int sy, int w, int h, int dx, int dy, bool useMask)
{
    if ( !m_rp || !bmp.IsOk() )
        return;
    // cut to the clip, moving the source with it
    int cx = dx, cy = dy, cw = w, ch = h;
    if ( !OLCut(cx, cy, cw, ch) )
        return;
    sx += cx - dx;
    sy += cy - dy;
    dx = cx; dy = cy; w = cw; h = ch;
    if ( sx < 0 || sy < 0 || sx + w > bmp.GetWidth() || sy + h > bmp.GetHeight() )
    {
        int x0 = wxMax(sx, 0), y0 = wxMax(sy, 0);
        int x1 = wxMin(sx + w, bmp.GetWidth()), y1 = wxMin(sy + h, bmp.GetHeight());
        if ( x1 <= x0 || y1 <= y0 )
            return;
        dx += x0 - sx; dy += y0 - sy;
        sx = x0; sy = y0; w = x1 - x0; h = y1 - y0;
    }

    wxBitmap& b = const_cast<wxBitmap&>(bmp);
    // alpha is always used, as wxGTK and wxMSW do; useMask is for the mask
    const bool alpha = b.HasAlpha();
    wxMask *mask = useMask ? b.GetMask() : NULL;
    const int bw = b.GetWidth();
    const int rx = m_ox + dx, ry = m_oy + dy;

    if ( !alpha && !mask && b.GetDepth() != 1 && m_logicalFunction == wxCOPY )
    {
        // straight copy: from its BitMap (drawn on by a memory DC, or made
        // once and kept), else its pixels
        struct BitMap *bm = b.GetBitMap();
        if ( bm )
        {
            BltBitMapRastPort(bm, sx, sy, m_rp, rx, ry, w, h, 0xc0);
            return;
        }
    }

    const unsigned char *src = b.GetPixels();
    if ( !src )
        return;
    if ( !m_olDirect )
    {
        // a palette screen: runs of a colour in its pen
        for ( int y = 0; y < h; y++ )
        {
            const unsigned char *row = src + ((size_t)(sy + y) * bw + sx) * 4;
            for ( int x = 0; x < w; )
            {
                const unsigned char *p = row + x * 4;
                if ( (alpha && p[0] < 128) )
                {
                    x++;
                    continue;
                }
                ULONG c = ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
                int e = x + 1;
                while ( e < w )
                {
                    const unsigned char *q = row + e * 4;
                    if ( (alpha && q[0] < 128) || (((ULONG)q[1] << 16) | ((ULONG)q[2] << 8) | q[3]) != c )
                        break;
                    e++;
                }
                SetAPen(m_rp, (ULONG)wxOLPenFor(c));
                RectFill(m_rp, rx + x, ry + y, rx + e - 1, ry + y);
                x = e;
            }
        }
        return;
    }

    if ( !alpha && !mask && b.GetDepth() != 1 )
    {
        wxOL_CGX_WRITE((APTR)src, sx, sy, bw * 4, m_rp, rx, ry, w, h);
        return;
    }

    // what is under it, the bitmap mixed in by its alpha or mask, put back
    ULONG *buf = (ULONG *)malloc((size_t)w * h * 4);
    if ( !buf )
        return;
    wxOL_CGX_READ(buf, 0, 0, w * 4, m_rp, rx, ry, w, h);
    const unsigned char *mp = NULL;
    int mw = 0;
    wxBitmap mb;
    if ( mask )
    {
        mb = mask->GetBitmap();
        mp = mb.IsOk() ? mb.GetPixels() : NULL;
        mw = mb.IsOk() ? mb.GetWidth() : 0;
    }
    for ( int y = 0; y < h; y++ )
    {
        const unsigned char *row = src + ((size_t)(sy + y) * bw + sx) * 4;
        ULONG *out = buf + (size_t)y * w;
        for ( int x = 0; x < w; x++ )
        {
            const unsigned char *p = row + x * 4;
            unsigned a = alpha ? p[0] : 255;
            if ( mp && !mp[((size_t)(sy + y) * mw + sx + x) * 4 + 1] )
                a = 0;
            if ( a == 0 )
                continue;
            if ( a == 255 )
            {
                out[x] = 0xff000000UL | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
                continue;
            }
            ULONG d = out[x];
            unsigned dr = (d >> 16) & 0xff, dg = (d >> 8) & 0xff, db = d & 0xff;
            dr += ((p[1] - (int)dr) * (int)a) / 255;
            dg += ((p[2] - (int)dg) * (int)a) / 255;
            db += ((p[3] - (int)db) * (int)a) / 255;
            out[x] = 0xff000000UL | (dr << 16) | (dg << 8) | db;
        }
    }
    wxOL_CGX_WRITE(buf, 0, 0, w * 4, m_rp, rx, ry, w, h);
    free(buf);
}

void wxOLDCImpl::DoDrawBitmap(const wxBitmap& bmp, wxCoord x, wxCoord y, bool useMask)
{
    if ( !bmp.IsOk() )
        return;
    if ( bmp.GetDepth() == 1 )
    {
        // a mono bitmap: the text colours (foreground where it is black)
        wxImage img = bmp.ConvertToImage();
        const wxColour fg = m_textForegroundColour, bg = m_textBackgroundColour;
        unsigned char *p = img.GetData();
        if ( useMask || m_backgroundMode != wxBRUSHSTYLE_SOLID )
            img.SetAlpha();
        unsigned char *a = img.HasAlpha() ? img.GetAlpha() : NULL;
        for ( int i = 0, n = img.GetWidth() * img.GetHeight(); i < n; i++, p += 3 )
        {
            bool on = p[0] < 128;
            const wxColour& c = on ? fg : bg;
            if ( a )
                a[i] = on ? 255 : 0;
            p[0] = c.Red(); p[1] = c.Green(); p[2] = c.Blue();
        }
        wxBitmap colour(img);
        OLPutBitmap(colour, 0, 0, colour.GetWidth(), colour.GetHeight(), LogicalToDeviceX(x), LogicalToDeviceY(y),
                    a != NULL);
    }
    else
        OLPutBitmap(bmp, 0, 0, bmp.GetWidth(), bmp.GetHeight(), LogicalToDeviceX(x), LogicalToDeviceY(y), useMask);
    CalcBoundingBox(x, y);
    CalcBoundingBox(x + bmp.GetWidth(), y + bmp.GetHeight());
}

void wxOLDCImpl::DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y)
{
    DoDrawBitmap(icon, x, y, true);
}

static ULONG MintermFor(wxRasterOperationMode rop)
{
    switch ( rop )
    {
        case wxCLEAR: return 0x00;
        case wxXOR: return 0x60;
        case wxINVERT: return 0x50;
        case wxOR_REVERSE: return 0xd0;
        case wxAND_REVERSE: return 0x40;
        case wxAND: return 0x80;
        case wxAND_INVERT: return 0x20;
        case wxNO_OP: return 0xa0;
        case wxNOR: return 0x10;
        case wxEQUIV: return 0x90;
        case wxSRC_INVERT: return 0x30;
        case wxOR_INVERT: return 0xb0;
        case wxNAND: return 0x70;
        case wxOR: return 0xe0;
        case wxSET: return 0xf0;
        case wxCOPY:
        default: return 0xc0;
    }
}

bool wxOLDCImpl::DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height, wxDC *source, wxCoord xsrc,
                        wxCoord ysrc, wxRasterOperationMode rop, bool useMask, wxCoord xsrcMask, wxCoord ysrcMask)
{
    wxUnusedVar(xsrcMask);
    wxUnusedVar(ysrcMask);
    if ( !m_rp || !source )
        return false;
    wxOLDCImpl *src = wxDynamicCast(source->GetImpl(), wxOLDCImpl);
    if ( !src || !src->m_rp )
        return false;
    int dx = LogicalToDeviceX(xdest), dy = LogicalToDeviceY(ydest);
    int w = LogicalToDeviceXRel(width), h = LogicalToDeviceYRel(height);
    int sx = src->LogicalToDeviceX(xsrc), sy = src->LogicalToDeviceY(ysrc);

    // a memory DC's bitmap with a mask or alpha: drawn mixed
    wxMemoryDCImpl *mem = wxDynamicCast(src, wxMemoryDCImpl);
    if ( mem && rop == wxCOPY )
    {
        wxBitmap& b = mem->GetSelectedBitmap();
        if ( b.IsOk() && ((useMask && b.GetMask()) || b.HasAlpha()) )
        {
            wxOLBitmapDrawnOn(b);
            OLPutBitmap(b, sx, sy, w, h, dx, dy, useMask);
            return true;
        }
    }

    int cx = dx, cy = dy, cw = w, ch = h;
    if ( !OLCut(cx, cy, cw, ch) )
        return true;
    sx += cx - dx;
    sy += cy - dy;
    // the source's own box
    if ( sx < 0 ) { cx -= sx; cw += sx; sx = 0; }
    if ( sy < 0 ) { cy -= sy; ch += sy; sy = 0; }
    if ( sx + cw > src->m_w ) cw = src->m_w - sx;
    if ( sy + ch > src->m_h ) ch = src->m_h - sy;
    if ( cw <= 0 || ch <= 0 )
        return true;
    const ULONG minterm = MintermFor(rop);
    if ( !src->m_rp->Layer )
        BltBitMapRastPort(src->m_rp->BitMap, src->m_ox + sx, src->m_oy + sy, m_rp, m_ox + cx, m_oy + cy, cw, ch, minterm);
    else
        ClipBlit(src->m_rp, src->m_ox + sx, src->m_oy + sy, m_rp, m_ox + cx, m_oy + cy, cw, ch, minterm);
    return true;
}

// ---- the window DCs -----------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxWindowDCImpl, wxOLDCImpl);

wxWindowDCImpl::wxWindowDCImpl(wxDC *owner, wxWindow *win)
    : wxOLDCImpl(owner), m_olShowAfter(false)
{
    OLInitForWindow(win, false);
}

wxWindowDCImpl::~wxWindowDCImpl()
{
}

void wxWindowDCImpl::OLInitForWindow(wxWindow *win, bool WXUNUSED(clientOnly))
{
    m_window = win;
    if ( !win )
        return;
    SetFont(win->GetFont());
    SetBackground(wxBrush(win->GetBackgroundColour()));
    m_textForegroundColour = win->GetForegroundColour();
    m_textBackgroundColour = win->GetBackgroundColour();

    // in a paint of this window: where olui wants it drawn
    if ( wxOLCurrentPaint && wxOLCurrentPaint->win == win )
    {
        OLInit(wxOLCurrentPaint->rp, wxOLCurrentPaint->x, wxOLCurrentPaint->y, wxOLCurrentPaint->w,
               wxOLCurrentPaint->h);
        return;
    }
    // otherwise straight into the Intuition window, where the window is
    wxNonOwnedWindow *tlw = win->OLGetTLW();
    struct Window *w = tlw ? tlw->OLGetWindow() : NULL;
    if ( !w )
        return;
    m_olBox = win->OLGetBoxInTLW();
    OLInit(w->RPort, w->BorderLeft + m_olBox.x, w->BorderTop + m_olBox.y, m_olBox.width, m_olBox.height);
}

wxIMPLEMENT_ABSTRACT_CLASS(wxClientDCImpl, wxWindowDCImpl);

wxClientDCImpl::wxClientDCImpl(wxDC *owner, wxWindow *win)
    : wxWindowDCImpl(owner)
{
    OLInitForWindow(win, true);
}

wxIMPLEMENT_ABSTRACT_CLASS(wxPaintDCImpl, wxClientDCImpl);

wxPaintDCImpl::wxPaintDCImpl(wxDC *owner, wxWindow *win)
    : wxClientDCImpl(owner)
{
    OLInitForWindow(win, true);
}

// ---- the memory DC --------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxMemoryDCImpl, wxOLDCImpl);

namespace
{
// a layer on a bitmap's BitMap, so drawing in it is clipped
struct LayerOnBitMap
{
    struct Layer_Info *li;
    struct Layer *layer;
};
}

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC *owner)
    : wxOLDCImpl(owner)
{
    Init();
}

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC *owner, wxBitmap& bitmap)
    : wxOLDCImpl(owner)
{
    Init();
    DoSelect(bitmap);
}

wxMemoryDCImpl::wxMemoryDCImpl(wxMemoryDC *owner, wxDC *WXUNUSED(dc))
    : wxOLDCImpl(owner)
{
    Init();
}

wxMemoryDCImpl::~wxMemoryDCImpl()
{
    OLRelease();
}

void wxMemoryDCImpl::Init()
{
    m_olRP = NULL;
    m_ok = false;
}

void wxMemoryDCImpl::OLRelease()
{
    OLRemoveClip();
    if ( m_olRP )
    {
        LayerOnBitMap *l = (LayerOnBitMap *)m_olRP;
        if ( l->layer )
            DeleteLayer(0, l->layer);
        if ( l->li )
            DisposeLayerInfo(l->li);
        FreeVec(l);
        m_olRP = NULL;
    }
    if ( m_bmp.IsOk() )
        wxOLBitmapDrawnOn(m_bmp);
    m_rp = NULL;
    m_ok = false;
}

void wxMemoryDCImpl::DoSelect(const wxBitmap& bitmap)
{
    OLRelease();
    m_bmp = bitmap;
    if ( !m_bmp.IsOk() )
        return;
    struct BitMap *bm = m_bmp.GetBitMap();
    if ( !bm )
        return;
    LayerOnBitMap *l = (LayerOnBitMap *)AllocVec(sizeof(LayerOnBitMap), MEMF_ANY | MEMF_CLEAR);
    if ( !l )
        return;
    l->li = NewLayerInfo();
    if ( l->li )
        // no backfill: a new layer would clear the bitmap's pixels to pen 0
        l->layer = CreateUpfrontHookLayer(l->li, bm, 0, 0, m_bmp.GetWidth() - 1, m_bmp.GetHeight() - 1, LAYERSIMPLE,
                                          LAYERS_NOBACKFILL, NULL);
    // m_olRP holds the layer pair; the DC draws in the layer's RastPort
    m_olRP = (struct RastPort *)l;
    if ( !l->layer )
        return;
    wxOLBitmapDrawnOn(m_bmp);
    OLInit(l->layer->rp, 0, 0, m_bmp.GetWidth(), m_bmp.GetHeight());
}

// ---- the screen DC -------------------------------------------------------------------------

wxIMPLEMENT_ABSTRACT_CLASS(wxScreenDCImpl, wxOLDCImpl);

wxScreenDCImpl::wxScreenDCImpl(wxScreenDC *owner)
    : wxOLDCImpl(owner)
{
    struct Screen *scr = wxOLGetScreen();
    if ( scr )
        OLInit(&scr->RastPort, 0, 0, scr->Width, scr->Height);
}
