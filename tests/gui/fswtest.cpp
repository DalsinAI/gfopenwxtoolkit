/* wxFileSystemWatcher on dos.library's notification: a folder in RAM:
 * watched as a tree, then a file made, written to, renamed and deleted, and
 * a folder made with a file in it. One line a check, and a last line
 * "fswtest: N of M passed". A folder other than RAM: can be given (the
 * test makes fswtest.dir in it):
 *   fswtest Work:
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/fswatcher.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <stdio.h>

static int g_pass = 0, g_total = 0;

static void Check(bool ok, const char *what)
{
    g_total++;
    if ( ok )
        g_pass++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fflush(stdout);
}

class App : public wxApp
{
public:
    App() : m_watcher(NULL), m_step(0), m_ticks(0) { }

    bool OnInit() wxOVERRIDE
    {
        const wxString base = argc > 1 ? argv[1] : wxString("RAM:");
        m_dir = base + (base.EndsWith(":") || base.EndsWith("/") ? "" : "/") + "fswtest.dir";
        wxFileName::Rmdir(m_dir, wxPATH_RMDIR_RECURSIVE);
        Check(wxFileName::Mkdir(m_dir), "test folder made");
        m_watcher = new wxFileSystemWatcher;
        m_watcher->SetOwner(this);
        Bind(wxEVT_FSWATCHER, &App::OnFS, this);
        Check(m_watcher->AddTree(wxFileName::DirName(m_dir)), "AddTree");
        Check(m_watcher->GetWatchedPathsCount() >= 1, "one path watched");
        m_timer.SetOwner(this);
        Bind(wxEVT_TIMER, &App::OnStep, this);
        m_timer.Start(200);
        return true;
    }

    int OnExit() wxOVERRIDE
    {
        delete m_watcher;
        m_watcher = NULL;
        wxFileName::Rmdir(m_dir, wxPATH_RMDIR_RECURSIVE);
        return wxApp::OnExit();
    }

private:
    wxString F(const char *name) const { return m_dir + "/" + name; }

    void OnFS(wxFileSystemWatcherEvent& ev)
    {
        const int t = ev.GetChangeType();
        wxString line = wxString::Format("%d %s", t, ev.GetPath().GetFullName());
        if ( t == wxFSW_EVENT_RENAME )
            line += " " + ev.GetNewPath().GetFullName();
        printf("     event: %s (%s)\n", (const char *)ev.ToString().mb_str(), (const char *)line.mb_str());
        fflush(stdout);
        m_seen.Add(line);
    }

    bool Saw(int type, const char *name, const char *newName = NULL) const
    {
        wxString line = wxString::Format("%d %s", type, name);
        if ( newName )
            line += wxString(" ") + newName;
        return m_seen.Index(line) != wxNOT_FOUND;
    }

    // each step does one thing, then waits up to 5 s for what it should
    // cause: a file system without notification is read every second or two
    struct Step { int type; const char *name, *newName, *what; };

    void Do(int step)
    {
        switch ( step )
        {
            case 0: { wxFile f(F("a.txt"), wxFile::write); f.Write("one"); break; }
            case 1: { wxFile f(F("a.txt"), wxFile::write_append); f.Write(" two, longer"); break; }
            case 2: wxRenameFile(F("a.txt"), F("b.txt")); break;
            case 3: wxRemoveFile(F("b.txt")); break;
            case 4: wxFileName::Mkdir(F("sub")); break;
            case 5: { wxFile f(F("sub/c.txt"), wxFile::write); f.Write("three"); break; }
            case 6:
                Check(m_watcher->RemoveAll(), "RemoveAll");
                m_seen.Clear();
                { wxFile f(F("d.txt"), wxFile::write); f.Write("four"); }
                break;
        }
    }

    void OnStep(wxTimerEvent& WXUNUSED(ev))
    {
        static const Step steps[] = {
            { wxFSW_EVENT_CREATE, "a.txt", NULL, "create a.txt" },
            { wxFSW_EVENT_MODIFY, "a.txt", NULL, "modify a.txt" },
            { wxFSW_EVENT_RENAME, "a.txt", "b.txt", "rename a.txt to b.txt" },
            { wxFSW_EVENT_DELETE, "b.txt", NULL, "delete b.txt" },
            { wxFSW_EVENT_CREATE, "sub", NULL, "create sub" },
            { wxFSW_EVENT_CREATE, "c.txt", NULL, "create sub/c.txt (a folder made under the tree is watched)" },
            { 0, "d.txt", NULL, "nothing after RemoveAll" },
        };
        const int n = sizeof steps / sizeof steps[0];
        if ( m_ticks == 0 )
        {
            if ( m_step == n )
            {
                m_timer.Stop();
                printf("fswtest: %d of %d passed\n", g_pass, g_total);
                fflush(stdout);
                ExitMainLoop();
                return;
            }
            Do(m_step);
        }
        const Step& s = steps[m_step];
        const bool none = s.type == 0;
        const bool seen = none ? Saw(wxFSW_EVENT_CREATE, s.name) : Saw(s.type, s.name, s.newName);
        // 25 ticks of 200 ms
        if ( (!none && seen) || ++m_ticks >= 25 || (none && seen) )
        {
            Check(none ? !seen : seen, s.what);
            m_step++;
            m_ticks = 0;
        }
    }

    wxFileSystemWatcher *m_watcher;
    wxTimer m_timer;
    wxString m_dir;
    wxArrayString m_seen;
    int m_step;
    int m_ticks;
};

wxIMPLEMENT_APP(App);
