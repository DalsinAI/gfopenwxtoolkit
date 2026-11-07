/* wxARexxPort: a frame with the port ARXTEST.1, one command of its own
 * (HELLO NAME/A, COUNT/N, LOUD/S) and the standard ones. Writes the port's
 * name to RAM:arexxtest.port once it is open; tests/amiga/arexxtest.rexx
 * then talks to it and QUITs it.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/openlayout/arexx.h>
#include <stdio.h>

class Frame : public wxFrame
{
public:
    Frame() : wxFrame(NULL, wxID_ANY, "arexxtest"), m_rexx("ARXTEST", "arexxtest 1.0")
    {
        m_rexx.Add("HELLO", "NAME/A,COUNT/N,LOUD/S", [](wxARexxCommand& c) {
            wxString s = "hello " + c.Str(0);
            if ( c.Has(1) )
                s += wxString::Format(" x%ld", c.Number(1));
            if ( c.Switch(2) )
                s.MakeUpper();
            c.SetResult(s);
        });
        m_rexx.Add("FAIL", "", [](wxARexxCommand& c) { c.SetError(10, 42); });
        // answers once a CallAfter has run (WaitFor runs queued events)
        m_rexx.Add("WAITCALL", "", [this](wxARexxCommand& c) {
            bool done = false;
            CallAfter([&done]() { done = true; });
            if ( m_rexx.WaitFor(c, [&done]() { return done; }, 2000) )
                c.SetResult("done");
        });
        // never ready: RC 10 after MS milliseconds
        m_rexx.Add("WAITNEVER", "MS/N/A", [this](wxARexxCommand& c) {
            m_rexx.WaitFor(c, []() { return false; }, c.Number(0));
        });
        FILE *f = fopen("RAM:arexxtest.port", "w");
        if ( f )
        {
            fputs(m_rexx.Open() ? (const char *)m_rexx.GetPortName().mb_str() : "NONE", f);
            fclose(f);
        }
    }

private:
    wxARexxPort m_rexx;
};

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        (new Frame)->Show();
        return true;
    }
};

wxIMPLEMENT_APP(App);
