// checkwrap: a checkbox and a radio button with long labels in a sizer (they
// wrapped one word to a line: a fixed 60-pixel width), and wxPlatformInfo's
// view of the system. It writes RAM:checkwrap.log and closes after SECONDS n
// (default 4; 0: until closed).
// Copyright (c) 2026 AmiCradle Team
#include <wx/wx.h>
#include <wx/platinfo.h>
#include <stdio.h>
#include <stdlib.h>

class Frame : public wxFrame
{
public:
    Frame(int seconds)
        : wxFrame(NULL, wxID_ANY, "checkwrap"), m_timer(this)
    {
        wxPanel *p = new wxPanel(this);
        wxBoxSizer *s = new wxBoxSizer(wxVERTICAL);
        wxCheckBox *c = new wxCheckBox(p, wxID_ANY, "Show this rather long checkbox label on one line");
        wxRadioButton *r = new wxRadioButton(p, wxID_ANY, "And this radio button's long label too", wxDefaultPosition,
                                             wxDefaultSize, wxRB_GROUP);
        s->Add(c, 0, wxALL, 6);
        s->Add(r, 0, wxALL, 6);
        p->SetSizer(s);
        s->SetSizeHints(this);

        FILE *f = fopen("RAM:checkwrap.log", "w");
        if ( f )
        {
            const wxSize cs = c->GetBestSize(), rs = r->GetBestSize();
            const int ch = c->GetCharHeight();
            fprintf(f, "checkbox best %dx%d radio best %dx%d char height %d\n", cs.x, cs.y, rs.x, rs.y, ch);
            fprintf(f, "%s\n", cs.x > 200 && cs.y < 2 * ch + 4 ? "checkbox one line" : "checkbox WRAPS");
            fprintf(f, "%s\n", rs.x > 180 && rs.y < 2 * ch + 4 ? "radio one line" : "radio WRAPS");
            const wxPlatformInfo& pi = wxPlatformInfo::Get();
            fprintf(f, "os id %d name '%s' family '%s' version %d.%d.%d ok %d\n", (int)pi.GetOperatingSystemId(),
                    (const char *)pi.GetOperatingSystemIdName().mb_str(),
                    (const char *)pi.GetOperatingSystemFamilyName().mb_str(), pi.GetOSMajorVersion(),
                    pi.GetOSMinorVersion(), pi.GetOSMicroVersion(), (int)pi.IsOk());
            fprintf(f, "description '%s'\n", (const char *)wxGetOsDescription().mb_str());
            fprintf(f, "%s\n", pi.GetOperatingSystemId() == wxOS_AMIGAOS ? "platform AmigaOS" : "platform NOT AmigaOS");
            fclose(f);
        }
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Close(true); });
        if ( seconds )
            m_timer.StartOnce(seconds * 1000);
    }

private:
    wxTimer m_timer;
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
        (new Frame(seconds))->Show();
        return true;
    }
};

wxIMPLEMENT_APP(App);
