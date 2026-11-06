/* Text measures for a font: what wxDC and wxWindow give for a fixed font
 * (wxFONTFAMILY_MODERN, and a face named on the command line, at 10 pt),
 * against the Amiga TextFont under it. Prints one line per font.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/dcmemory.h>
#include <graphics/text.h>
#include <stdio.h>

static void Measure(wxWindow *w, const wxFont& f, const char *what)
{
    wxBitmap bmp(200, 50);
    wxMemoryDC dc(bmp);
    dc.SetFont(f);
    wxCoord d0w, d0h, d10w, d10h, ww, wh;
    dc.GetTextExtent("0", &d0w, &d0h);
    dc.GetTextExtent("0123456789", &d10w, &d10h);
    dc.GetTextExtent("W", &ww, &wh);
    w->SetFont(f);
    struct TextFont *tf = f.GetTextFont();
    printf("fonttest: %s: face '%s' %d pt fixed %d | dc char %d x %d, '0' %d, 10 digits %d, 'W' %d | window char %d x %d | TextFont %s %d px xsize %d %s\n",
           what, (const char *)f.GetFaceName().mb_str(), f.GetPointSize(), f.IsFixedWidth(),
           (int)dc.GetCharWidth(), (int)dc.GetCharHeight(), (int)d0w, (int)d10w, (int)ww,
           w->GetCharWidth(), w->GetCharHeight(),
           tf && tf->tf_Message.mn_Node.ln_Name ? tf->tf_Message.mn_Node.ln_Name : "(none)",
           tf ? tf->tf_YSize : 0, tf ? tf->tf_XSize : 0,
           tf ? ((tf->tf_Flags & FPF_PROPORTIONAL) ? "proportional" : "fixed") : "");
}

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        wxFrame *fr = new wxFrame(NULL, wxID_ANY, "fonttest");
        wxPanel *p = new wxPanel(fr);
        Measure(p, wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL), "modern");
        for (int i = 1; i < argc; i++)
            Measure(p, wxFont(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, argv[i]),
                    (const char *)argv[i].mb_str());
        Measure(p, *wxNORMAL_FONT, "normal");
        fr->Destroy();
        return false;
    }
};

wxIMPLEMENT_APP(App);
