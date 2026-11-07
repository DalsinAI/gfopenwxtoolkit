/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/glcanvas.h
// Purpose:     wxGLCanvas and wxGLContext on GFOpenGPU's GL (GLA)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_GLCANVAS_H_
#define _WX_OPENLAYOUT_GLCANVAS_H_

// OpenGL comes from GFOpenGPU's GL module (Mesa: virgl on AmiCradle's GPU,
// softpipe on the 68k elsewhere) through libGL.a. A context is a GLA
// context; a canvas has a GLA buffer of its size, whose frames go straight
// into the Intuition window at the canvas's place. The canvas is a box
// GFOpenLayout's buffer is copied around (olui_amiga_own), and each time
// the window is drawn there the canvas gets a paint event to draw itself.
#include <GL/gl.h>

struct gla_context;
struct gla_buffer;
struct wxGLTarget;

class WXDLLIMPEXP_GL wxGLContext : public wxGLContextBase
{
public:
    wxGLContext(wxGLCanvas *win,
                const wxGLContext *other = NULL,
                const wxGLContextAttrs *ctxAttrs = NULL);
    virtual ~wxGLContext();

    virtual bool SetCurrent(const wxGLCanvas& win) const wxOVERRIDE;

    struct gla_context *OLGetGLA() const { return m_gla; }

private:
    struct gla_context *m_gla;

    wxDECLARE_CLASS(wxGLContext);
};

// ----------------------------------------------------------------------------
// wxGLCanvas: OpenGL output window
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_GL wxGLCanvas : public wxGLCanvasBase
{
public:
    wxGLCanvas() { Init(); }

    explicit // avoid implicitly converting a wxWindow* to wxGLCanvas
    wxGLCanvas(wxWindow *parent,
               const wxGLAttributes& dispAttrs,
               wxWindowID id = wxID_ANY,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const wxPalette& palette = wxNullPalette);

    explicit
    wxGLCanvas(wxWindow *parent,
               wxWindowID id = wxID_ANY,
               const int *attribList = NULL,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxGLCanvasName,
               const wxPalette& palette = wxNullPalette);

    virtual ~wxGLCanvas();

    bool Create(wxWindow *parent,
                const wxGLAttributes& dispAttrs,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxGLCanvasName,
                const wxPalette& palette = wxNullPalette);

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxGLCanvasName,
                const int *attribList = NULL,
                const wxPalette& palette = wxNullPalette);

    virtual bool SwapBuffers() wxOVERRIDE;

    // ---- the port's own -------------------------------------------------
    virtual bool OLOwnDrawn() const wxOVERRIDE { return true; }
    virtual void OLDrawOwn(int x, int y, int w, int h) wxOVERRIDE;
    // the GLA buffer, made (or sized again) for the canvas's size now
    struct gla_buffer *OLGetBuffer();
    // the context SetCurrent last made current on it
    void OLSetContext(const wxGLContext *ctx) const { m_olContext = ctx; }
    // what the canvas asked for: GLA's double buffer, depth and stencil bits
    bool OLDoubleBuffer() const { return m_olDouble; }
    int OLDepth() const { return m_olDepth; }
    int OLStencil() const { return m_olStencil; }
    int OLAlpha() const { return m_olAlpha; }

private:
    void Init();
    void OLTakeAttributes(const int *attrs);
    void OLAim();

    struct gla_buffer *m_olBuffer;
    struct wxGLTarget *m_olTarget;     // where its frames go: the window's RastPort, the box's corner
    mutable const wxGLContext *m_olContext;
    int m_olW, m_olH;
    bool m_olDouble;
    int m_olDepth, m_olStencil, m_olAlpha;
    bool m_olSeen;                      // olui has drawn its box at least once

    wxDECLARE_CLASS(wxGLCanvas);
};

#endif // _WX_OPENLAYOUT_GLCANVAS_H_
