/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/graphics.cpp
// Purpose:     wxGraphicsContext for the openlayout port: paths, pens,
//              brushes (solid, linear and radial gradients), text and
//              bitmaps drawn through the wxDC the context was made for.
//              Curves and arcs are flattened to lines, transforms are full
//              affine; gradients are filled span by span. There is no alpha
//              blending: a colour with alpha 0 draws nothing, any other
//              draws opaque. Antialiasing is not done.
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/graphics.h"
#include "wx/private/graphics.h"
#include "wx/dc.h"
#include "wx/dcclient.h"
#include "wx/dcmemory.h"
#include "wx/window.h"
#include "wx/image.h"
#include "wx/bitmap.h"
#include "wx/icon.h"
#include "wx/region.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

const double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// matrix: x' = a*x + c*y + tx, y' = b*x + d*y + ty
// ---------------------------------------------------------------------------

class OLMatrixData : public wxGraphicsMatrixData
{
public:
    explicit OLMatrixData(wxGraphicsRenderer *r) : wxGraphicsMatrixData(r) {}

    wxGraphicsObjectRefData *Clone() const wxOVERRIDE
    {
        OLMatrixData *m = new OLMatrixData(m_renderer);
        m->m_a = m_a; m->m_b = m_b; m->m_c = m_c; m->m_d = m_d;
        m->m_tx = m_tx; m->m_ty = m_ty;
        return m;
    }

    void Concat(const wxGraphicsMatrixData *t) wxOVERRIDE
    {
        // this = t applied first, then this
        const OLMatrixData *o = static_cast<const OLMatrixData *>(t);
        double a = o->m_a * m_a + o->m_b * m_c;
        double b = o->m_a * m_b + o->m_b * m_d;
        double c = o->m_c * m_a + o->m_d * m_c;
        double d = o->m_c * m_b + o->m_d * m_d;
        double tx = o->m_tx * m_a + o->m_ty * m_c + m_tx;
        double ty = o->m_tx * m_b + o->m_ty * m_d + m_ty;
        m_a = a; m_b = b; m_c = c; m_d = d; m_tx = tx; m_ty = ty;
    }

    void Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
             wxDouble tx, wxDouble ty) wxOVERRIDE
    {
        m_a = a; m_b = b; m_c = c; m_d = d; m_tx = tx; m_ty = ty;
    }

    void Get(wxDouble *a, wxDouble *b, wxDouble *c, wxDouble *d,
             wxDouble *tx, wxDouble *ty) const wxOVERRIDE
    {
        if (a) *a = m_a;
        if (b) *b = m_b;
        if (c) *c = m_c;
        if (d) *d = m_d;
        if (tx) *tx = m_tx;
        if (ty) *ty = m_ty;
    }

    void Invert() wxOVERRIDE
    {
        double det = m_a * m_d - m_b * m_c;
        if (det == 0)
            return;
        double a = m_d / det, b = -m_b / det, c = -m_c / det, d = m_a / det;
        double tx = -(a * m_tx + c * m_ty);
        double ty = -(b * m_tx + d * m_ty);
        m_a = a; m_b = b; m_c = c; m_d = d; m_tx = tx; m_ty = ty;
    }

    bool IsEqual(const wxGraphicsMatrixData *t) const wxOVERRIDE
    {
        const OLMatrixData *o = static_cast<const OLMatrixData *>(t);
        return m_a == o->m_a && m_b == o->m_b && m_c == o->m_c &&
               m_d == o->m_d && m_tx == o->m_tx && m_ty == o->m_ty;
    }

    bool IsIdentity() const wxOVERRIDE
    {
        return m_a == 1 && m_b == 0 && m_c == 0 && m_d == 1 && m_tx == 0 && m_ty == 0;
    }

    void Translate(wxDouble dx, wxDouble dy) wxOVERRIDE
    {
        m_tx += m_a * dx + m_c * dy;
        m_ty += m_b * dx + m_d * dy;
    }

    void Scale(wxDouble sx, wxDouble sy) wxOVERRIDE
    {
        m_a *= sx; m_b *= sx;
        m_c *= sy; m_d *= sy;
    }

    void Rotate(wxDouble angle) wxOVERRIDE
    {
        double cs = std::cos(angle), sn = std::sin(angle);
        double a = m_a * cs + m_c * sn;
        double b = m_b * cs + m_d * sn;
        double c = -m_a * sn + m_c * cs;
        double d = -m_b * sn + m_d * cs;
        m_a = a; m_b = b; m_c = c; m_d = d;
    }

    void TransformPoint(wxDouble *x, wxDouble *y) const wxOVERRIDE
    {
        double nx = m_a * *x + m_c * *y + m_tx;
        double ny = m_b * *x + m_d * *y + m_ty;
        *x = nx; *y = ny;
    }

    void TransformDistance(wxDouble *dx, wxDouble *dy) const wxOVERRIDE
    {
        double nx = m_a * *dx + m_c * *dy;
        double ny = m_b * *dx + m_d * *dy;
        *dx = nx; *dy = ny;
    }

    void *GetNativeMatrix() const wxOVERRIDE { return const_cast<OLMatrixData *>(this); }

    double m_a = 1, m_b = 0, m_c = 0, m_d = 1, m_tx = 0, m_ty = 0;
};

// ---------------------------------------------------------------------------
// path: subpaths of points, curves flattened as they are added
// ---------------------------------------------------------------------------

struct OLSubpath
{
    std::vector<wxPoint2DDouble> pts;
    bool closed = false;
};

class OLPathData : public wxGraphicsPathData
{
public:
    explicit OLPathData(wxGraphicsRenderer *r) : wxGraphicsPathData(r) {}

    wxGraphicsObjectRefData *Clone() const wxOVERRIDE
    {
        OLPathData *p = new OLPathData(m_renderer);
        p->m_subs = m_subs;
        p->m_cur = m_cur;
        p->m_hasCur = m_hasCur;
        return p;
    }

    void MoveToPoint(wxDouble x, wxDouble y) wxOVERRIDE
    {
        m_subs.push_back(OLSubpath());
        m_subs.back().pts.push_back(wxPoint2DDouble(x, y));
        m_cur = wxPoint2DDouble(x, y);
        m_hasCur = true;
    }

    void AddLineToPoint(wxDouble x, wxDouble y) wxOVERRIDE
    {
        if (!m_hasCur || m_subs.empty() || m_subs.back().closed) {
            MoveToPoint(m_hasCur ? m_cur.m_x : x, m_hasCur ? m_cur.m_y : y);
        }
        m_subs.back().pts.push_back(wxPoint2DDouble(x, y));
        m_cur = wxPoint2DDouble(x, y);
    }

    void AddCurveToPoint(wxDouble cx1, wxDouble cy1, wxDouble cx2, wxDouble cy2,
                         wxDouble x, wxDouble y) wxOVERRIDE
    {
        if (!m_hasCur)
            MoveToPoint(cx1, cy1);
        const wxPoint2DDouble p0 = m_cur;
        const int steps = 16;
        for (int i = 1; i <= steps; ++i) {
            double t = double(i) / steps, u = 1 - t;
            double bx = u * u * u * p0.m_x + 3 * u * u * t * cx1 + 3 * u * t * t * cx2 + t * t * t * x;
            double by = u * u * u * p0.m_y + 3 * u * u * t * cy1 + 3 * u * t * t * cy2 + t * t * t * y;
            AddLineToPoint(bx, by);
        }
    }

    void AddPath(const wxGraphicsPathData *path) wxOVERRIDE
    {
        const OLPathData *o = static_cast<const OLPathData *>(path);
        m_subs.insert(m_subs.end(), o->m_subs.begin(), o->m_subs.end());
        if (o->m_hasCur) {
            m_cur = o->m_cur;
            m_hasCur = true;
        }
    }

    void CloseSubpath() wxOVERRIDE
    {
        if (m_subs.empty())
            return;
        m_subs.back().closed = true;
        if (!m_subs.back().pts.empty())
            m_cur = m_subs.back().pts.front();
    }

    void GetCurrentPoint(wxDouble *x, wxDouble *y) const wxOVERRIDE
    {
        if (x) *x = m_cur.m_x;
        if (y) *y = m_cur.m_y;
    }

    void AddArc(wxDouble x, wxDouble y, wxDouble r, wxDouble startAngle,
                wxDouble endAngle, bool clockwise) wxOVERRIDE
    {
        // angles in radians, clockwise on screen (y down) when clockwise
        double sweep = endAngle - startAngle;
        if (clockwise) {
            while (sweep < 0) sweep += 2 * kPi;
            if (sweep > 2 * kPi) sweep = 2 * kPi;
        } else {
            while (sweep > 0) sweep -= 2 * kPi;
            if (sweep < -2 * kPi) sweep = -2 * kPi;
        }
        int steps = std::max(4, int(std::fabs(sweep) / (2 * kPi) * 48 + 0.5));
        double sx = x + r * std::cos(startAngle), sy = y + r * std::sin(startAngle);
        if (m_hasCur && !m_subs.empty() && !m_subs.back().closed)
            AddLineToPoint(sx, sy);
        else
            MoveToPoint(sx, sy);
        for (int i = 1; i <= steps; ++i) {
            double a = startAngle + sweep * i / steps;
            AddLineToPoint(x + r * std::cos(a), y + r * std::sin(a));
        }
    }

    void *GetNativePath() const wxOVERRIDE { return const_cast<OLPathData *>(this); }
    void UnGetNativePath(void *) const wxOVERRIDE {}

    void Transform(const wxGraphicsMatrixData *matrix) wxOVERRIDE
    {
        for (auto &s : m_subs)
            for (auto &p : s.pts)
                matrix->TransformPoint(&p.m_x, &p.m_y);
        matrix->TransformPoint(&m_cur.m_x, &m_cur.m_y);
    }

    void GetBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) const wxOVERRIDE
    {
        double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool any = false;
        for (const auto &s : m_subs)
            for (const auto &p : s.pts) {
                if (!any) { x0 = x1 = p.m_x; y0 = y1 = p.m_y; any = true; }
                x0 = std::min(x0, p.m_x); x1 = std::max(x1, p.m_x);
                y0 = std::min(y0, p.m_y); y1 = std::max(y1, p.m_y);
            }
        if (x) *x = x0;
        if (y) *y = y0;
        if (w) *w = x1 - x0;
        if (h) *h = y1 - y0;
    }

    bool Contains(wxDouble x, wxDouble y, wxPolygonFillMode fillStyle) const wxOVERRIDE
    {
        int winding = 0, crossings = 0;
        for (const auto &s : m_subs) {
            size_t n = s.pts.size();
            for (size_t i = 0; i < n; ++i) {
                const wxPoint2DDouble &a = s.pts[i];
                const wxPoint2DDouble &b = s.pts[(i + 1) % n];
                if ((a.m_y <= y) != (b.m_y <= y)) {
                    double xi = a.m_x + (y - a.m_y) * (b.m_x - a.m_x) / (b.m_y - a.m_y);
                    if (xi > x) {
                        ++crossings;
                        winding += b.m_y > a.m_y ? 1 : -1;
                    }
                }
            }
        }
        return fillStyle == wxWINDING_RULE ? winding != 0 : (crossings & 1) != 0;
    }

    std::vector<OLSubpath> m_subs;
    wxPoint2DDouble m_cur;
    bool m_hasCur = false;
};

// ---------------------------------------------------------------------------
// pens, brushes, fonts, bitmaps
// ---------------------------------------------------------------------------

class OLPenData : public wxGraphicsObjectRefData
{
public:
    OLPenData(wxGraphicsRenderer *r, const wxGraphicsPenInfo &info)
        : wxGraphicsObjectRefData(r), m_colour(info.GetColour()),
          m_width(info.GetWidth()), m_style(info.GetStyle()) {}
    wxColour m_colour;
    double m_width;
    wxPenStyle m_style;
};

enum OLBrushKind { OLBrushSolid, OLBrushLinear, OLBrushRadial };

class OLBrushData : public wxGraphicsObjectRefData
{
public:
    explicit OLBrushData(wxGraphicsRenderer *r) : wxGraphicsObjectRefData(r) {}

    // colour at a gradient position 0..1
    wxColour At(double t) const
    {
        if (m_stops.empty())
            return m_colour;
        t = std::max(0.0, std::min(1.0, t));
        if (t <= m_stops.front().first)
            return m_stops.front().second;
        for (size_t i = 1; i < m_stops.size(); ++i) {
            if (t <= m_stops[i].first) {
                double t0 = m_stops[i - 1].first, t1 = m_stops[i].first;
                double f = t1 > t0 ? (t - t0) / (t1 - t0) : 0;
                const wxColour &a = m_stops[i - 1].second, &b = m_stops[i].second;
                return wxColour(
                    (unsigned char)(a.Red() + (b.Red() - a.Red()) * f + 0.5),
                    (unsigned char)(a.Green() + (b.Green() - a.Green()) * f + 0.5),
                    (unsigned char)(a.Blue() + (b.Blue() - a.Blue()) * f + 0.5),
                    (unsigned char)(a.Alpha() + (b.Alpha() - a.Alpha()) * f + 0.5));
            }
        }
        return m_stops.back().second;
    }

    // colour of the brush at a user-space point
    wxColour AtPoint(double x, double y) const
    {
        if (m_kind == OLBrushLinear) {
            double dx = m_x2 - m_x1, dy = m_y2 - m_y1;
            double len2 = dx * dx + dy * dy;
            double t = len2 > 0 ? ((x - m_x1) * dx + (y - m_y1) * dy) / len2 : 0;
            return At(t);
        }
        if (m_kind == OLBrushRadial) {
            double dx = x - m_x2, dy = y - m_y2;
            double t = m_radius > 0 ? std::sqrt(dx * dx + dy * dy) / m_radius : 0;
            return At(t);
        }
        return m_colour;
    }

    OLBrushKind m_kind = OLBrushSolid;
    wxColour m_colour;
    bool m_transparent = false;
    std::vector<std::pair<double, wxColour>> m_stops;
    double m_x1 = 0, m_y1 = 0, m_x2 = 0, m_y2 = 0, m_radius = 0;
    // the brush's own transform (user space to gradient space), if any
    bool m_hasMatrix = false;
    OLMatrixData *m_inverse = nullptr;

    ~OLBrushData() { if (m_inverse) m_inverse->DecRef(); }
};

class OLFontData : public wxGraphicsObjectRefData
{
public:
    OLFontData(wxGraphicsRenderer *r, const wxFont &font, const wxColour &col)
        : wxGraphicsObjectRefData(r), m_font(font), m_colour(col) {}
    wxFont m_font;
    wxColour m_colour;
};

class OLBitmapData : public wxGraphicsBitmapData
{
public:
    OLBitmapData(wxGraphicsRenderer *r, const wxBitmap &bmp)
        : wxGraphicsBitmapData(r), m_bitmap(bmp) {}
    void *GetNativeBitmap() const wxOVERRIDE { return const_cast<wxBitmap *>(&m_bitmap); }
    wxBitmap m_bitmap;
};

// ---------------------------------------------------------------------------
// the context
// ---------------------------------------------------------------------------

struct OLState
{
    OLMatrixData *matrix;
    bool clipped;
    wxRect clip;
};

class OLContext : public wxGraphicsContext
{
public:
    // draws through dc; owns it when ownDC
    OLContext(wxGraphicsRenderer *r, wxDC *dc, bool ownDC, wxWindow *win = NULL)
        : wxGraphicsContext(r, win), m_dc(dc), m_ownDC(ownDC)
    {
        m_matrix = new OLMatrixData(r);
        int w = 0, h = 0;
        if (m_dc)
            m_dc->GetSize(&w, &h);
        m_width = w;
        m_height = h;
    }

    // a context on an image: drawn into a bitmap, copied back when done
    OLContext(wxGraphicsRenderer *r, wxImage &image)
        : wxGraphicsContext(r), m_image(&image)
    {
        m_imageBitmap = wxBitmap(image);
        wxMemoryDC *mdc = new wxMemoryDC(m_imageBitmap);
        m_dc = mdc;
        m_ownDC = true;
        m_matrix = new OLMatrixData(r);
        m_width = image.GetWidth();
        m_height = image.GetHeight();
    }

    ~OLContext()
    {
        for (auto &s : m_stack)
            s.matrix->DecRef();
        m_matrix->DecRef();
        if (m_image && m_dc) {
            static_cast<wxMemoryDC *>(m_dc)->SelectObject(wxNullBitmap);
            *m_image = m_imageBitmap.ConvertToImage();
        }
        if (m_ownDC)
            delete m_dc;
    }

    void PushState() wxOVERRIDE
    {
        OLState s;
        s.matrix = static_cast<OLMatrixData *>(m_matrix->Clone());
        s.clipped = m_clipped;
        s.clip = m_clip;
        m_stack.push_back(s);
    }

    void PopState() wxOVERRIDE
    {
        if (m_stack.empty())
            return;
        OLState s = m_stack.back();
        m_stack.pop_back();
        m_matrix->DecRef();
        m_matrix = s.matrix;
        m_clipped = s.clipped;
        m_clip = s.clip;
        ApplyClip();
    }

    void Clip(const wxRegion &region) wxOVERRIDE
    {
        wxRect box = region.GetBox();
        Clip(box.x, box.y, box.width, box.height);
    }

    void Clip(wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE
    {
        // the transformed rectangle's bounding box
        double xs[4] = { x, x + w, x, x + w }, ys[4] = { y, y, y + h, y + h };
        double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
        for (int i = 0; i < 4; ++i) {
            m_matrix->TransformPoint(&xs[i], &ys[i]);
            x0 = std::min(x0, xs[i]); x1 = std::max(x1, xs[i]);
            y0 = std::min(y0, ys[i]); y1 = std::max(y1, ys[i]);
        }
        wxRect r(int(std::floor(x0)), int(std::floor(y0)),
                 int(std::ceil(x1) - std::floor(x0)), int(std::ceil(y1) - std::floor(y0)));
        m_clip = m_clipped ? m_clip.Intersect(r) : r;
        m_clipped = true;
        ApplyClip();
    }

    void ResetClip() wxOVERRIDE
    {
        m_clipped = false;
        ApplyClip();
    }

    void GetClipBox(wxDouble *x, wxDouble *y, wxDouble *w, wxDouble *h) wxOVERRIDE
    {
        wxRect r = m_clipped ? m_clip : wxRect(0, 0, int(m_width), int(m_height));
        // back into user space (bounding box)
        OLMatrixData *inv = static_cast<OLMatrixData *>(m_matrix->Clone());
        inv->Invert();
        double xs[4] = { double(r.x), double(r.GetRight() + 1), double(r.x), double(r.GetRight() + 1) };
        double ys[4] = { double(r.y), double(r.y), double(r.GetBottom() + 1), double(r.GetBottom() + 1) };
        double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
        for (int i = 0; i < 4; ++i) {
            inv->TransformPoint(&xs[i], &ys[i]);
            x0 = std::min(x0, xs[i]); x1 = std::max(x1, xs[i]);
            y0 = std::min(y0, ys[i]); y1 = std::max(y1, ys[i]);
        }
        inv->DecRef();
        if (x) *x = x0;
        if (y) *y = y0;
        if (w) *w = x1 - x0;
        if (h) *h = y1 - y0;
    }

    void *GetNativeContext() wxOVERRIDE { return m_dc; }

    bool SetAntialiasMode(wxAntialiasMode antialias) wxOVERRIDE
    {
        m_antialias = antialias;
        return true;
    }

    bool SetInterpolationQuality(wxInterpolationQuality q) wxOVERRIDE
    {
        m_interpolation = q;
        return true;
    }

    bool SetCompositionMode(wxCompositionMode op) wxOVERRIDE
    {
        m_composition = op;
        return op == wxCOMPOSITION_OVER || op == wxCOMPOSITION_SOURCE;
    }

    void BeginLayer(wxDouble) wxOVERRIDE {}
    void EndLayer() wxOVERRIDE {}

    void Translate(wxDouble dx, wxDouble dy) wxOVERRIDE { m_matrix->Translate(dx, dy); }
    void Scale(wxDouble sx, wxDouble sy) wxOVERRIDE { m_matrix->Scale(sx, sy); }
    void Rotate(wxDouble angle) wxOVERRIDE { m_matrix->Rotate(angle); }

    void ConcatTransform(const wxGraphicsMatrix &matrix) wxOVERRIDE
    {
        m_matrix->Concat(static_cast<const OLMatrixData *>(matrix.GetMatrixData()));
    }

    void SetTransform(const wxGraphicsMatrix &matrix) wxOVERRIDE
    {
        const OLMatrixData *o = static_cast<const OLMatrixData *>(matrix.GetMatrixData());
        if (o)
            m_matrix->Set(o->m_a, o->m_b, o->m_c, o->m_d, o->m_tx, o->m_ty);
        else
            m_matrix->Set(1, 0, 0, 1, 0, 0);
    }

    wxGraphicsMatrix GetTransform() const wxOVERRIDE
    {
        wxGraphicsMatrix m;
        m.SetRefData(m_matrix->Clone());
        return m;
    }

    void StrokePath(const wxGraphicsPath &path) wxOVERRIDE
    {
        if (!m_dc || m_pen.IsNull())
            return;
        const OLPenData *pen = static_cast<const OLPenData *>(m_pen.GetRefData());
        if (pen->m_style == wxPENSTYLE_TRANSPARENT || pen->m_colour.Alpha() == 0)
            return;
        const OLPathData *p = static_cast<const OLPathData *>(path.GetPathData());
        double wx_ = pen->m_width, wy_ = 0;
        m_matrix->TransformDistance(&wx_, &wy_);
        int width = std::max(1, int(std::sqrt(wx_ * wx_ + wy_ * wy_) + 0.5));
        m_dc->SetPen(wxPen(Opaque(pen->m_colour), width, pen->m_style));
        for (const auto &s : p->m_subs) {
            std::vector<wxPoint> pts = Device(s.pts);
            if (s.closed && !pts.empty())
                pts.push_back(pts.front());
            if (pts.size() == 1)
                m_dc->DrawPoint(pts[0]);
            else if (pts.size() > 1)
                m_dc->DrawLines(int(pts.size()), pts.data());
        }
    }

    void FillPath(const wxGraphicsPath &path, wxPolygonFillMode fillStyle) wxOVERRIDE
    {
        if (!m_dc || m_brush.IsNull())
            return;
        const OLBrushData *brush = static_cast<const OLBrushData *>(m_brush.GetRefData());
        if (brush->m_transparent)
            return;
        const OLPathData *p = static_cast<const OLPathData *>(path.GetPathData());

        std::vector<std::vector<wxPoint2DDouble>> polys;
        for (const auto &s : p->m_subs) {
            if (s.pts.size() < 3)
                continue;
            std::vector<wxPoint2DDouble> dev(s.pts);
            for (auto &q : dev)
                m_matrix->TransformPoint(&q.m_x, &q.m_y);
            polys.push_back(dev);
        }
        if (polys.empty())
            return;

        if (brush->m_kind == OLBrushSolid) {
            if (brush->m_colour.Alpha() == 0)
                return;
            m_dc->SetPen(*wxTRANSPARENT_PEN);
            m_dc->SetBrush(wxBrush(Opaque(brush->m_colour)));
            std::vector<int> counts;
            std::vector<wxPoint> pts;
            for (const auto &poly : polys) {
                counts.push_back(int(poly.size()));
                for (const auto &q : poly)
                    pts.push_back(wxPoint(int(std::floor(q.m_x + 0.5)), int(std::floor(q.m_y + 0.5))));
            }
            if (counts.size() == 1)
                m_dc->DrawPolygon(counts[0], pts.data(), 0, 0, fillStyle);
            else
                m_dc->DrawPolyPolygon(int(counts.size()), counts.data(), pts.data(), 0, 0, fillStyle);
            return;
        }
        FillGradient(polys, *brush, fillStyle);
    }

    void GetTextExtent(const wxString &text, wxDouble *width, wxDouble *height,
                       wxDouble *descent, wxDouble *externalLeading) const wxOVERRIDE
    {
        wxCoord w = 0, h = 0, d = 0, e = 0;
        if (m_dc) {
            UseFont();
            m_dc->GetTextExtent(text, &w, &h, &d, &e);
        }
        if (width) *width = w;
        if (height) *height = h;
        if (descent) *descent = d;
        if (externalLeading) *externalLeading = e;
    }

    void GetPartialTextExtents(const wxString &text, wxArrayDouble &widths) const wxOVERRIDE
    {
        widths.clear();
        if (!m_dc)
            return;
        UseFont();
        wxArrayInt iw;
        m_dc->GetPartialTextExtents(text, iw);
        for (int v : iw)
            widths.push_back(v);
    }

    void DrawBitmap(const wxGraphicsBitmap &bmp, wxDouble x, wxDouble y,
                    wxDouble w, wxDouble h) wxOVERRIDE
    {
        const OLBitmapData *b = static_cast<const OLBitmapData *>(bmp.GetRefData());
        if (b)
            DrawBitmap(b->m_bitmap, x, y, w, h);
    }

    void DrawBitmap(const wxBitmap &bmp, wxDouble x, wxDouble y,
                    wxDouble w, wxDouble h) wxOVERRIDE
    {
        if (!m_dc || !bmp.IsOk())
            return;
        double x0 = x, y0 = y, x1 = x + w, y1 = y + h;
        m_matrix->TransformPoint(&x0, &y0);
        m_matrix->TransformPoint(&x1, &y1);
        int dw = int(std::fabs(x1 - x0) + 0.5), dh = int(std::fabs(y1 - y0) + 0.5);
        if (dw <= 0 || dh <= 0)
            return;
        int dx = int(std::floor(std::min(x0, x1) + 0.5)), dy = int(std::floor(std::min(y0, y1) + 0.5));
        if (dw == bmp.GetWidth() && dh == bmp.GetHeight()) {
            m_dc->DrawBitmap(bmp, dx, dy, true);
        } else {
            wxImage img = bmp.ConvertToImage();
            img.Rescale(dw, dh, m_interpolation == wxINTERPOLATION_NONE
                                    ? wxIMAGE_QUALITY_NEAREST : wxIMAGE_QUALITY_NORMAL);
            m_dc->DrawBitmap(wxBitmap(img), dx, dy, true);
        }
    }

    void DrawIcon(const wxIcon &icon, wxDouble x, wxDouble y, wxDouble w, wxDouble h) wxOVERRIDE
    {
        wxBitmap bmp;
        bmp.CopyFromIcon(icon);
        DrawBitmap(bmp, x, y, w, h);
    }

protected:
    void DoDrawText(const wxString &str, wxDouble x, wxDouble y) wxOVERRIDE
    {
        if (!m_dc || str.empty())
            return;
        UseFont();
        double dx = x, dy = y;
        m_matrix->TransformPoint(&dx, &dy);
        m_dc->DrawText(str, int(std::floor(dx + 0.5)), int(std::floor(dy + 0.5)));
    }

private:
    static wxColour Opaque(const wxColour &c)
    {
        return wxColour(c.Red(), c.Green(), c.Blue());
    }

    std::vector<wxPoint> Device(const std::vector<wxPoint2DDouble> &in) const
    {
        std::vector<wxPoint> out;
        out.reserve(in.size());
        for (auto q : in) {
            m_matrix->TransformPoint(&q.m_x, &q.m_y);
            out.push_back(wxPoint(int(std::floor(q.m_x + 0.5)), int(std::floor(q.m_y + 0.5))));
        }
        return out;
    }

    void UseFont() const
    {
        if (m_font.IsNull())
            return;
        const OLFontData *f = static_cast<const OLFontData *>(m_font.GetRefData());
        m_dc->SetFont(f->m_font);
        m_dc->SetTextForeground(f->m_colour);
        m_dc->SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
    }

    void ApplyClip()
    {
        if (!m_dc)
            return;
        m_dc->DestroyClippingRegion();
        if (m_clipped)
            m_dc->SetClippingRegion(m_clip);
    }

    // scanline fill of device-space polygons, the colour taken from the
    // brush (in user space) every few pixels along each span
    void FillGradient(const std::vector<std::vector<wxPoint2DDouble>> &polys,
                      const OLBrushData &brush, wxPolygonFillMode fillStyle)
    {
        double ymin = 1e30, ymax = -1e30;
        for (const auto &poly : polys)
            for (const auto &q : poly) {
                ymin = std::min(ymin, q.m_y);
                ymax = std::max(ymax, q.m_y);
            }
        OLMatrixData *inv = static_cast<OLMatrixData *>(m_matrix->Clone());
        inv->Invert();
        const int step = 4;
        struct Edge { double x; int dir; };
        std::vector<Edge> xs;
        for (int y = int(std::floor(ymin)); y <= int(std::ceil(ymax)); ++y) {
            const double sy = y + 0.5;
            xs.clear();
            for (const auto &poly : polys) {
                size_t n = poly.size();
                for (size_t i = 0; i < n; ++i) {
                    const wxPoint2DDouble &a = poly[i], &b = poly[(i + 1) % n];
                    if ((a.m_y <= sy) != (b.m_y <= sy)) {
                        double xi = a.m_x + (sy - a.m_y) * (b.m_x - a.m_x) / (b.m_y - a.m_y);
                        xs.push_back({ xi, b.m_y > a.m_y ? 1 : -1 });
                    }
                }
            }
            std::sort(xs.begin(), xs.end(), [](const Edge &l, const Edge &r) { return l.x < r.x; });
            int wind = 0;
            for (size_t i = 0; i + 1 < xs.size(); ++i) {
                wind += xs[i].dir;
                bool inside = fillStyle == wxWINDING_RULE ? wind != 0 : (i % 2) == 0;
                if (!inside)
                    continue;
                int x0 = int(std::floor(xs[i].x + 0.5)), x1 = int(std::floor(xs[i + 1].x + 0.5));
                for (int x = x0; x < x1; x += step) {
                    int xe = std::min(x1, x + step);
                    double ux = (x + xe) * 0.5, uy = sy;
                    inv->TransformPoint(&ux, &uy);
                    double gx = ux, gy = uy;
                    if (brush.m_hasMatrix && brush.m_inverse)
                        brush.m_inverse->TransformPoint(&gx, &gy);
                    wxColour c = brush.AtPoint(gx, gy);
                    if (c.Alpha() == 0)
                        continue;
                    m_dc->SetPen(wxPen(Opaque(c)));
                    m_dc->DrawLine(x, y, xe, y);
                }
            }
        }
        inv->DecRef();
    }

    wxDC *m_dc = NULL;
    bool m_ownDC = false;
    wxImage *m_image = NULL;
    wxBitmap m_imageBitmap;
    OLMatrixData *m_matrix = NULL;
    std::vector<OLState> m_stack;
    bool m_clipped = false;
    wxRect m_clip;
};

// ---------------------------------------------------------------------------
// the renderer
// ---------------------------------------------------------------------------

class OLRenderer : public wxGraphicsRenderer
{
public:
    wxGraphicsContext *CreateContext(const wxWindowDC &dc) wxOVERRIDE
    {
        return new OLContext(this, const_cast<wxWindowDC *>(&dc), false, dc.GetWindow());
    }

    wxGraphicsContext *CreateContext(const wxMemoryDC &dc) wxOVERRIDE
    {
        return new OLContext(this, const_cast<wxMemoryDC *>(&dc), false);
    }

#if wxUSE_PRINTING_ARCHITECTURE
    wxGraphicsContext *CreateContext(const wxPrinterDC &) wxOVERRIDE { return NULL; }
#endif

    wxGraphicsContext *CreateContextFromNativeContext(void *context) wxOVERRIDE
    {
        // our native context is a wxDC
        return new OLContext(this, static_cast<wxDC *>(context), false);
    }

    wxGraphicsContext *CreateContextFromNativeWindow(void *window) wxOVERRIDE
    {
        return CreateContext(static_cast<wxWindow *>(window));
    }

    wxGraphicsContext *CreateContext(wxWindow *window) wxOVERRIDE
    {
        return new OLContext(this, new wxClientDC(window), true, window);
    }

#if wxUSE_IMAGE
    wxGraphicsContext *CreateContextFromImage(wxImage &image) wxOVERRIDE
    {
        return new OLContext(this, image);
    }
#endif

    wxGraphicsContext *CreateMeasuringContext() wxOVERRIDE
    {
        // a small bitmap to measure text on
        static wxBitmap s_bmp(8, 8);
        wxMemoryDC *mdc = new wxMemoryDC(s_bmp);
        return new OLContext(this, mdc, true);
    }

    wxGraphicsPath CreatePath() wxOVERRIDE
    {
        wxGraphicsPath p;
        p.SetRefData(new OLPathData(this));
        return p;
    }

    wxGraphicsMatrix CreateMatrix(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                                  wxDouble tx, wxDouble ty) wxOVERRIDE
    {
        OLMatrixData *m = new OLMatrixData(this);
        m->Set(a, b, c, d, tx, ty);
        wxGraphicsMatrix gm;
        gm.SetRefData(m);
        return gm;
    }

    wxGraphicsPen CreatePen(const wxGraphicsPenInfo &info) wxOVERRIDE
    {
        if (info.GetStyle() == wxPENSTYLE_TRANSPARENT)
            return wxNullGraphicsPen;
        wxGraphicsPen p;
        p.SetRefData(new OLPenData(this, info));
        return p;
    }

    wxGraphicsBrush CreateBrush(const wxBrush &brush) wxOVERRIDE
    {
        if (!brush.IsOk() || brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT)
            return wxNullGraphicsBrush;
        OLBrushData *b = new OLBrushData(this);
        b->m_colour = brush.GetColour();
        wxGraphicsBrush gb;
        gb.SetRefData(b);
        return gb;
    }

    wxGraphicsBrush CreateLinearGradientBrush(wxDouble x1, wxDouble y1, wxDouble x2, wxDouble y2,
                                              const wxGraphicsGradientStops &stops,
                                              const wxGraphicsMatrix &matrix) wxOVERRIDE
    {
        OLBrushData *b = Gradient(stops, matrix);
        b->m_kind = OLBrushLinear;
        b->m_x1 = x1; b->m_y1 = y1; b->m_x2 = x2; b->m_y2 = y2;
        wxGraphicsBrush gb;
        gb.SetRefData(b);
        return gb;
    }

    wxGraphicsBrush CreateRadialGradientBrush(wxDouble startX, wxDouble startY,
                                              wxDouble endX, wxDouble endY, wxDouble radius,
                                              const wxGraphicsGradientStops &stops,
                                              const wxGraphicsMatrix &matrix) wxOVERRIDE
    {
        OLBrushData *b = Gradient(stops, matrix);
        b->m_kind = OLBrushRadial;
        b->m_x1 = startX; b->m_y1 = startY; b->m_x2 = endX; b->m_y2 = endY;
        b->m_radius = radius;
        wxGraphicsBrush gb;
        gb.SetRefData(b);
        return gb;
    }

    wxGraphicsFont CreateFont(const wxFont &font, const wxColour &col) wxOVERRIDE
    {
        if (!font.IsOk())
            return wxNullGraphicsFont;
        wxGraphicsFont f;
        f.SetRefData(new OLFontData(this, font, col));
        return f;
    }

    wxGraphicsFont CreateFont(double sizeInPixels, const wxString &facename, int flags,
                              const wxColour &col) wxOVERRIDE
    {
        wxFontInfo info = wxFontInfo().Family(wxFONTFAMILY_DEFAULT).FaceName(facename);
        info.AllFlags(flags);
        wxFont font(info);
        font.SetPixelSize(wxSize(0, int(sizeInPixels + 0.5)));
        return CreateFont(font, col);
    }

    wxGraphicsFont CreateFontAtDPI(const wxFont &font, const wxRealPoint &, const wxColour &col) wxOVERRIDE
    {
        return CreateFont(font, col);
    }

    wxGraphicsBitmap CreateBitmap(const wxBitmap &bitmap) wxOVERRIDE
    {
        if (!bitmap.IsOk())
            return wxNullGraphicsBitmap;
        wxGraphicsBitmap b;
        b.SetRefData(new OLBitmapData(this, bitmap));
        return b;
    }

#if wxUSE_IMAGE
    wxGraphicsBitmap CreateBitmapFromImage(const wxImage &image) wxOVERRIDE
    {
        return CreateBitmap(wxBitmap(image));
    }

    wxImage CreateImageFromBitmap(const wxGraphicsBitmap &bmp) wxOVERRIDE
    {
        const OLBitmapData *b = static_cast<const OLBitmapData *>(bmp.GetRefData());
        return b ? b->m_bitmap.ConvertToImage() : wxImage();
    }
#endif

    wxGraphicsBitmap CreateBitmapFromNativeBitmap(void *bitmap) wxOVERRIDE
    {
        return bitmap ? CreateBitmap(*static_cast<wxBitmap *>(bitmap)) : wxNullGraphicsBitmap;
    }

    wxGraphicsBitmap CreateSubBitmap(const wxGraphicsBitmap &bitmap, wxDouble x, wxDouble y,
                                     wxDouble w, wxDouble h) wxOVERRIDE
    {
        const OLBitmapData *b = static_cast<const OLBitmapData *>(bitmap.GetRefData());
        if (!b)
            return wxNullGraphicsBitmap;
        return CreateBitmap(b->m_bitmap.GetSubBitmap(wxRect(int(x), int(y), int(w), int(h))));
    }

    wxString GetName() const wxOVERRIDE { return wxS("openlayout"); }

    void GetVersion(int *major, int *minor, int *micro) const wxOVERRIDE
    {
        if (major) *major = 1;
        if (minor) *minor = 0;
        if (micro) *micro = 0;
    }

private:
    OLBrushData *Gradient(const wxGraphicsGradientStops &stops, const wxGraphicsMatrix &matrix)
    {
        OLBrushData *b = new OLBrushData(this);
        for (unsigned i = 0; i < stops.GetCount(); ++i)
            b->m_stops.push_back({ stops.Item(i).GetPosition(), stops.Item(i).GetColour() });
        std::stable_sort(b->m_stops.begin(), b->m_stops.end(),
                         [](const std::pair<double, wxColour> &l, const std::pair<double, wxColour> &r)
                         { return l.first < r.first; });
        b->m_colour = stops.GetStartColour();
        if (!matrix.IsNull()) {
            const OLMatrixData *m = static_cast<const OLMatrixData *>(matrix.GetMatrixData());
            b->m_inverse = static_cast<OLMatrixData *>(m->Clone());
            b->m_inverse->Invert();
            b->m_hasMatrix = true;
        }
        return b;
    }
};

} // anonymous namespace

wxGraphicsRenderer *wxGraphicsRenderer::GetDefaultRenderer()
{
    static OLRenderer s_renderer;
    return &s_renderer;
}

// wx declares it for every port; there is no Cairo here
wxGraphicsRenderer *wxGraphicsRenderer::GetCairoRenderer()
{
    return NULL;
}

#endif // wxUSE_GRAPHICS_CONTEXT
