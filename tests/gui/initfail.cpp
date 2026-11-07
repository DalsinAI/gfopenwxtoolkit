/* A program that gives up in OnInit after telling the user why (as Audacity
 * does when its temporary folder can't be written): it must end, not hang.
 *   initfail box    a message box, then OnInit returns false
 *   initfail frame  a frame made (not shown), a message box, then false
 *   initfail log    wxLogError, then false (wx shows the log at the end)
 * Writes RAM:initfail.<step> as it goes; RAM:initfail.9gone when the app
 * object is destroyed.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <stdio.h>

static void Step(const char *n)
{
    wxString name = wxString("RAM:initfail.") + n;
    FILE *f = fopen((const char *)name.mb_str(), "w");
    if (f)
        fclose(f);
}

class App : public wxApp
{
public:
    ~App() { Step("9gone"); }
    bool OnInit() wxOVERRIDE
    {
        const wxString mode = argc > 1 ? wxString(argv[1]) : wxString("box");
        Step("1init");
        if ( mode == "frame" )
            new wxFrame(NULL, wxID_ANY, "initfail");
        if ( mode == "log" )
            wxLogError("The temporary folder can't be written.");
        else
            wxMessageBox("The temporary folder can't be written.", "initfail", wxOK | wxICON_ERROR);
        Step("2told");
        return false;
    }
    int OnExit() wxOVERRIDE
    {
        Step("3exit");
        return wxApp::OnExit();
    }
};

wxIMPLEMENT_APP(App);
