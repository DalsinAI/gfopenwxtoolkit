// glspin: a wxGLCanvas in a frame, a triangle turning on a timer, for the
// lab. It writes RAM:glspin.log: GL's renderer and version, the colour GL
// read back from the middle of the canvas, the frames drawn, and "done".
//   glspin [SECONDS n]     (default 4; 0: until the window is closed)
// Copyright (c) 2026 AmiCradle Team
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static FILE *gs_log;

static void Log(const char *fmt, ...)
{
    if ( !gs_log )
        gs_log = fopen("RAM:glspin.log", "w");
    if ( !gs_log )
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(gs_log, fmt, ap);
    va_end(ap);
    fflush(gs_log);
}

class Canvas : public wxGLCanvas
{
public:
    Canvas(wxWindow *parent, const wxGLAttributes& a)
        : wxGLCanvas(parent, a), m_ctx(this), m_angle(0), m_frames(0), m_told(false)
    {
        Bind(wxEVT_PAINT, &Canvas::OnPaint, this);
    }
    void Turn() { m_angle += 10; Refresh(false); }
    int Frames() const { return m_frames; }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxPaintDC dc(this);
        if ( !m_ctx.IsOK() || !SetCurrent(m_ctx) )
        {
            if ( !m_told )
                Log("no GL context\n");
            m_told = true;
            return;
        }
        const wxSize sz = GetClientSize();
        glViewport(0, 0, sz.x, sz.y);
        glClearColor(0.1f, 0.1f, 0.3f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef((float)m_angle, 0, 0, 1);
        glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0); glVertex2f(0, 0.9f);
        glColor3f(0, 1, 0); glVertex2f(-0.8f, -0.7f);
        glColor3f(0, 0, 1); glVertex2f(0.8f, -0.7f);
        glEnd();
        if ( !m_told )
        {
            unsigned char px[4] = { 0, 0, 0, 0 }, corner[4] = { 0, 0, 0, 0 };
            glReadPixels(sz.x / 2, sz.y / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, corner);
            Log("renderer %s\nversion %s\nsize %dx%d\nmiddle %d,%d,%d corner %d,%d,%d\n",
                (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION), sz.x, sz.y,
                px[0], px[1], px[2], corner[0], corner[1], corner[2]);
            m_told = true;
        }
        SwapBuffers();
        m_frames++;
    }

    wxGLContext m_ctx;
    int m_angle, m_frames;
    bool m_told;
};

class Frame : public wxFrame
{
public:
    Frame(int seconds)
        : wxFrame(NULL, wxID_ANY, "glspin", wxDefaultPosition, wxSize(340, 300)), m_timer(this), m_ticks(0),
          m_seconds(seconds)
    {
        wxGLAttributes a;
        a.PlatformDefaults().Defaults().EndList();
        Log("display %s\n", wxGLCanvas::IsDisplaySupported(a) ? "yes" : "no");
        wxPanel *p = new wxPanel(this);
        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        s->Add(new wxStaticText(p, wxID_ANY, "A triangle turning in a wxGLCanvas:"), 0, wxALL, 4);
        m_canvas = new Canvas(p, a);
        s->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);
        p->SetSizer(s);
        Bind(wxEVT_TIMER, &Frame::OnTimer, this);
        m_timer.Start(100);
    }

private:
    void OnTimer(wxTimerEvent&)
    {
        m_canvas->Turn();
        if ( m_seconds && ++m_ticks >= m_seconds * 10 )
        {
            m_timer.Stop();
            Log("frames %d\ndone\n", m_canvas->Frames());
            Close(true);
        }
    }

    Canvas *m_canvas;
    wxTimer m_timer;
    int m_ticks, m_seconds;
};

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        int seconds = 4;
        for ( int i = 1; i + 1 < argc; i++ )
            if ( wxString(argv[i]).CmpNoCase("SECONDS") == 0 )
                seconds = atoi(wxString(argv[i + 1]).mb_str());
        remove("RAM:glspin.log");
        (new Frame(seconds))->Show();
        return true;
    }
    int OnExit() wxOVERRIDE
    {
        if ( gs_log )
            fclose(gs_log);
        return 0;
    }
};

wxIMPLEMENT_APP(App);
