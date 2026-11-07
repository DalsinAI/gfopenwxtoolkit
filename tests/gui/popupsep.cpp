/* A pop-up menu with separators, opened by itself once the frame shows (as a
 * right click opens a context menu): the separators must be lines, and Down
 * from the first item must pass over them. Writes RAM:popupsep.<choice>.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <stdio.h>

static void Step(const wxString& n)
{
    wxString name = "RAM:popupsep." + n;
    FILE *f = fopen((const char *)name.mb_str(), "w");
    if (f)
        fclose(f);
}

class Frame : public wxFrame
{
public:
    Frame() : wxFrame(NULL, wxID_ANY, "popupsep", wxDefaultPosition, wxSize(320, 200))
    {
        CallAfter(&Frame::Open);
    }
    void Open()
    {
        wxMenu m;
        m.Append(101, "Cut");
        m.Append(102, "Copy");
        m.AppendSeparator();
        m.Append(103, "Paste");
        m.AppendSeparator();
        m.Append(104, "Select All");
        const int id = GetPopupMenuSelectionFromUser(m, wxPoint(20, 20));
        Step(wxString::Format("%d", id));
        Close();
    }
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
