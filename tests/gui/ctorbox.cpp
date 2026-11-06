/* A message box from a frame's constructor, before the main loop runs (as
 * wxHexEditor's licence box): it must take OK, Return and its close
 * gadget. Writes RAM:ctorbox.<step> as it goes.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <stdio.h>

static void Step(const char *n)
{
    wxString name = wxString("RAM:ctorbox.") + n;
    FILE *f = fopen((const char *)name.mb_str(), "w");
    if (f)
        fclose(f);
}

class Frame : public wxFrame
{
public:
    Frame() : wxFrame(NULL, wxID_ANY, "ctorbox")
    {
        Step("1ctor");
        int r = wxMessageBox("Press OK", "Licence", wxOK);
        Step(r == wxOK ? "2ok" : "2other");
    }
};

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        Frame *f = new Frame;
        Step("3shown");
        f->Show();
        f->Close();
        return true;
    }
};

wxIMPLEMENT_APP(App);
