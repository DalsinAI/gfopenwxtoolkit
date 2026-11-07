/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout-gl/glcanvas.cpp
// Purpose:     wxGLCanvas and wxGLContext on GFOpenGPU's GL (GLA): the
//              openlayout port's GL library, libwx_openlayoutu_gl
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GLCANVAS

#include "wx/glcanvas.h"
#include "wx/module.h"
#include "wx/nonownedwnd.h"

#include <string.h>
#include <stdlib.h>

#include <intuition/intuition.h>

// GLA, GFOpenGPU's own GL interface (the GFOpenGPU developer kit's
// include/gla): displays, contexts, and buffers whose frames a present hook
// shows; on OS 3.2 the hook writes them into a RastPort
#include <gla/gla_core.h>
extern "C" {
#include <gla/os3/gla_present_os3.h>
#include <gla/os3/gla_virgl_os3.h>
}

// where a canvas's frames go: the Intuition window's RastPort, at the
// corner of the canvas's box there (set each time olui draws the box)
struct wxGLTarget
{
    struct gla_os3_target target;
    struct gla_present hook;
};

// one display for the program: virgl on AmiCradle's GPU when the host
// answers, else softpipe on this CPU (SetEnv WXGL_CPU 1: softpipe always)
static struct gla_display *gs_display = NULL;
// what is current, so a canvas or context going away lets go of it first
static struct gla_context *gs_curContext = NULL;
static struct gla_buffer *gs_curBuffer = NULL;

static struct gla_display *GetDisplay()
{
    if ( gs_display )
        return gs_display;
    const char *cpu = getenv("WXGL_CPU");
    if ( !cpu || !*cpu || *cpu == '0' )
    {
        struct gla_virgl_transport t;
        if ( gla_os3_virgl_transport(&t) )
            gs_display = gla_display_create_virgl(&t);
    }
    if ( !gs_display )
        gs_display = gla_display_create();
    return gs_display;
}

static void LetGo(struct gla_context *c, struct gla_buffer *b)
{
    if ( (c && c == gs_curContext) || (b && b == gs_curBuffer) )
    {
        gla_make_current(NULL, NULL);
        gs_curContext = NULL;
        gs_curBuffer = NULL;
    }
}

class wxGLModule : public wxModule
{
public:
    virtual bool OnInit() wxOVERRIDE { return true; }
    virtual void OnExit() wxOVERRIDE
    {
        LetGo(gs_curContext, gs_curBuffer);
        if ( gs_display )
        {
            gla_display_destroy(gs_display);
            gs_display = NULL;
        }
        // (what presenting opened, GL.module closes with the program)
    }

private:
    wxDECLARE_DYNAMIC_CLASS(wxGLModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxGLModule, wxModule);

// ----------------------------------------------------------------------------
// the attribute lists: wx's own WX_GL_ numbers, each with its value (1 for
// those that have none), read back by the canvas and the context
// ----------------------------------------------------------------------------

static void Put(wxGLAttribsBase& a, int what, int value)
{
    a.AddAttribute(what);
    a.AddAttribute(value);
}

// the value given for what in a list made here, or def
static int Get(const int *attrs, int what, int def)
{
    if ( !attrs )
        return def;
    for ( ; attrs[0]; attrs += 2 )
        if ( attrs[0] == what )
            return attrs[1];
    return def;
}

wxGLContextAttrs& wxGLContextAttrs::CoreProfile() { Put(*this, WX_GL_CORE_PROFILE, 1); return *this; }
wxGLContextAttrs& wxGLContextAttrs::MajorVersion(int val) { if ( val > 0 ) Put(*this, WX_GL_MAJOR_VERSION, val); return *this; }
wxGLContextAttrs& wxGLContextAttrs::MinorVersion(int val) { if ( val >= 0 ) Put(*this, WX_GL_MINOR_VERSION, val); return *this; }
wxGLContextAttrs& wxGLContextAttrs::CompatibilityProfile() { Put(*this, WX_GL_COMPAT_PROFILE, 1); return *this; }
wxGLContextAttrs& wxGLContextAttrs::ForwardCompatible() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::ES2() { Put(*this, WX_GL_ES2, 1); return *this; }
wxGLContextAttrs& wxGLContextAttrs::DebugCtx() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::Robust() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::NoResetNotify() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::LoseOnReset() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::ResetIsolation() { return *this; }
wxGLContextAttrs& wxGLContextAttrs::ReleaseFlush(int WXUNUSED(val)) { return *this; }
wxGLContextAttrs& wxGLContextAttrs::PlatformDefaults() { renderTypeRGBA = true; x11Direct = true; return *this; }
void wxGLContextAttrs::EndList() { AddAttribute(0); }

wxGLAttributes& wxGLAttributes::RGBA() { return *this; }
wxGLAttributes& wxGLAttributes::BufferSize(int WXUNUSED(val)) { return *this; }
wxGLAttributes& wxGLAttributes::Level(int WXUNUSED(val)) { return *this; }
wxGLAttributes& wxGLAttributes::DoubleBuffer() { Put(*this, WX_GL_DOUBLEBUFFER, 1); return *this; }
wxGLAttributes& wxGLAttributes::Stereo() { return *this; }
wxGLAttributes& wxGLAttributes::AuxBuffers(int WXUNUSED(val)) { return *this; }
wxGLAttributes& wxGLAttributes::MinRGBA(int WXUNUSED(mRed), int WXUNUSED(mGreen), int WXUNUSED(mBlue), int mAlpha)
{
    if ( mAlpha > 0 )
        Put(*this, WX_GL_MIN_ALPHA, mAlpha);
    return *this;
}
wxGLAttributes& wxGLAttributes::Depth(int val) { if ( val >= 0 ) Put(*this, WX_GL_DEPTH_SIZE, val); return *this; }
wxGLAttributes& wxGLAttributes::Stencil(int val) { if ( val >= 0 ) Put(*this, WX_GL_STENCIL_SIZE, val); return *this; }
wxGLAttributes& wxGLAttributes::MinAcumRGBA(int WXUNUSED(mRed), int WXUNUSED(mGreen), int WXUNUSED(mBlue),
                                            int WXUNUSED(mAlpha)) { return *this; }
wxGLAttributes& wxGLAttributes::SampleBuffers(int WXUNUSED(val)) { return *this; }
wxGLAttributes& wxGLAttributes::Samplers(int WXUNUSED(val)) { return *this; }
wxGLAttributes& wxGLAttributes::FrameBuffersRGB() { return *this; }
void wxGLAttributes::EndList() { AddAttribute(0); }
wxGLAttributes& wxGLAttributes::PlatformDefaults() { return *this; }
wxGLAttributes& wxGLAttributes::Defaults()
{
    RGBA().DoubleBuffer().Depth(16);
    return *this;
}
void wxGLAttributes::AddDefaultsForWXBefore31()
{
    Defaults();
}

// GLA takes 0, 16 or 24 bits of depth and 0 or 8 of stencil
static int DepthBits(int bits) { return bits <= 0 ? 0 : bits <= 16 ? 16 : 24; }
static int StencilBits(int bits) { return bits > 0 ? 8 : 0; }

// ----------------------------------------------------------------------------
// wxGLContext
// ----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGLContext, wxObject);

wxGLContext::wxGLContext(wxGLCanvas *win, const wxGLContext *other, const wxGLContextAttrs *ctxAttrs)
    : m_gla(NULL)
{
    m_isOk = false;
    const int *a = ctxAttrs ? ctxAttrs->GetGLAttrs() : (win ? win->GetGLCTXAttrs().GetGLAttrs() : NULL);
    struct gla_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.profile = Get(a, WX_GL_ES2, 0) ? GLA_ES2 : Get(a, WX_GL_CORE_PROFILE, 0) ? GLA_CORE : GLA_COMPAT;
    cfg.major = Get(a, WX_GL_MAJOR_VERSION, 0);
    cfg.minor = cfg.major ? Get(a, WX_GL_MINOR_VERSION, 0) : 0;
    cfg.doublebuf = win ? win->OLDoubleBuffer() : 1;
    cfg.depth = win ? win->OLDepth() : 16;
    cfg.stencil = win ? win->OLStencil() : 0;
    cfg.alpha = win ? win->OLAlpha() : 0;
    struct gla_display *d = GetDisplay();
    if ( d )
        m_gla = gla_context_create(d, &cfg, other ? other->m_gla : NULL);
    m_isOk = m_gla != NULL;
    if ( !m_isOk )
        wxLogDebug("wxGLContext: GL couldn't start (no LIBS:GFOpenGPU/GL.module, or memory)");
}

wxGLContext::~wxGLContext()
{
    if ( m_gla )
    {
        LetGo(m_gla, NULL);
        gla_context_destroy(m_gla);
    }
}

bool wxGLContext::SetCurrent(const wxGLCanvas& win) const
{
    if ( !m_gla )
        return false;
    struct gla_buffer *b = const_cast<wxGLCanvas&>(win).OLGetBuffer();
    if ( !b || !gla_make_current(m_gla, b) )
        return false;
    gs_curContext = m_gla;
    gs_curBuffer = b;
    win.OLSetContext(this);
    return true;
}

// ----------------------------------------------------------------------------
// wxGLCanvas
// ----------------------------------------------------------------------------

wxIMPLEMENT_CLASS(wxGLCanvas, wxWindow);

void wxGLCanvas::Init()
{
    m_olBuffer = NULL;
    m_olTarget = NULL;
    m_olContext = NULL;
    m_olW = m_olH = 0;
    m_olDouble = true;
    m_olDepth = 16;
    m_olStencil = 0;
    m_olAlpha = 0;
    m_olSeen = false;
}

wxGLCanvas::wxGLCanvas(wxWindow *parent, const wxGLAttributes& dispAttrs, wxWindowID id, const wxPoint& pos,
                       const wxSize& size, long style, const wxString& name, const wxPalette& palette)
{
    Init();
    Create(parent, dispAttrs, id, pos, size, style, name, palette);
}

wxGLCanvas::wxGLCanvas(wxWindow *parent, wxWindowID id, const int *attribList, const wxPoint& pos,
                       const wxSize& size, long style, const wxString& name, const wxPalette& palette)
{
    Init();
    Create(parent, id, pos, size, style, name, attribList, palette);
}

bool wxGLCanvas::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style,
                        const wxString& name, const int *attribList, const wxPalette& palette)
{
    // the old list: wx's common code reads it into both kinds of attributes
    wxGLAttributes dispAttrs;
    if ( !ParseAttribList(attribList, dispAttrs, &m_GLCTXAttrs) )
        return false;
    return Create(parent, dispAttrs, id, pos, size, style, name, palette);
}

bool wxGLCanvas::Create(wxWindow *parent, const wxGLAttributes& dispAttrs, wxWindowID id, const wxPoint& pos,
                        const wxSize& size, long style, const wxString& name,
                        const wxPalette& WXUNUSED(palette))
{
    OLTakeAttributes(dispAttrs.GetGLAttrs());
    m_olTarget = new wxGLTarget;
    memset(m_olTarget, 0, sizeof *m_olTarget);
    gla_os3_present_init(&m_olTarget->hook, &m_olTarget->target);
    // (wxWindow's Create makes the element: a box olui copies around)
    return wxWindow::Create(parent, id, pos, size, style | wxFULL_REPAINT_ON_RESIZE, name);
}

void wxGLCanvas::OLTakeAttributes(const int *attrs)
{
    m_olDouble = Get(attrs, WX_GL_DOUBLEBUFFER, attrs ? 0 : 1) != 0;
    m_olDepth = DepthBits(Get(attrs, WX_GL_DEPTH_SIZE, attrs ? 0 : 16));
    m_olStencil = StencilBits(Get(attrs, WX_GL_STENCIL_SIZE, 0));
    m_olAlpha = Get(attrs, WX_GL_MIN_ALPHA, 0) > 0;
}

wxGLCanvas::~wxGLCanvas()
{
    // olui gives the box back with the element (wxWindow's destructor)
    if ( m_olBuffer )
    {
        LetGo(NULL, m_olBuffer);
        gla_buffer_destroy(m_olBuffer);
    }
    delete m_olTarget;
}

struct gla_buffer *wxGLCanvas::OLGetBuffer()
{
    int w, h;
    GetClientSize(&w, &h);
    if ( w < 1 )
        w = 1;
    if ( h < 1 )
        h = 1;
    if ( !m_olBuffer )
    {
        struct gla_display *d = GetDisplay();
        if ( !d || !m_olTarget )
            return NULL;
        struct gla_config cfg;
        memset(&cfg, 0, sizeof cfg);
        cfg.profile = GLA_COMPAT;
        cfg.doublebuf = m_olDouble;
        cfg.depth = m_olDepth;
        cfg.stencil = m_olStencil;
        cfg.alpha = m_olAlpha;
        m_olBuffer = gla_buffer_create(d, &cfg, (unsigned)w, (unsigned)h, &m_olTarget->hook);
        m_olW = w;
        m_olH = h;
    }
    else if ( w != m_olW || h != m_olH )
    {
        gla_buffer_resize(m_olBuffer, (unsigned)w, (unsigned)h);
        m_olW = w;
        m_olH = h;
    }
    return m_olBuffer;
}

// points the frames at the window, where olui drew the box last
void wxGLCanvas::OLAim()
{
    if ( !m_olTarget )
        return;
    struct Window *win = m_olTLW ? m_olTLW->OLGetWindow() : NULL;
    if ( !win || !m_olSeen || !IsShownOnScreen() )
        m_olTarget->target.rp = NULL;
    else
        m_olTarget->target.rp = win->RPort;
}

void wxGLCanvas::OLDrawOwn(int x, int y, int w, int h)
{
    struct Window *win = m_olTLW ? m_olTLW->OLGetWindow() : NULL;
    if ( !win || !m_olTarget )
        return;
    m_olTarget->target.left = x;
    m_olTarget->target.top = y;
    m_olSeen = true;
    OLAim();
    // the program draws it in its paint handler (SetCurrent, GL, SwapBuffers)
    OLPaint(win->RPort, x, y, w, h);
}

bool wxGLCanvas::SwapBuffers()
{
    if ( !m_olContext || !m_olBuffer || !m_olContext->OLGetGLA() )
        return false;
    OLAim();
    gla_swap(m_olContext->OLGetGLA(), m_olBuffer);
    return true;
}

// ----------------------------------------------------------------------------
// what wx's common code asks of every port
// ----------------------------------------------------------------------------

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const wxGLAttributes& WXUNUSED(dispAttrs))
{
    return GetDisplay() != NULL;
}

/* static */
bool wxGLCanvasBase::IsDisplaySupported(const int *WXUNUSED(attribList))
{
    return GetDisplay() != NULL;
}

/* static */
bool wxGLCanvasBase::IsExtensionSupported(const char *extension)
{
    // the current context's (GL needs one to answer)
    if ( !gs_curContext )
        return false;
    return IsExtensionInList((const char *)glGetString(GL_EXTENSIONS), extension);
}

bool wxGLApp::InitGLVisual(const int *WXUNUSED(attribList))
{
    return GetDisplay() != NULL;
}

#endif // wxUSE_GLCANVAS
