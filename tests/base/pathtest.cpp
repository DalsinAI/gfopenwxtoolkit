// pathtest: Amiga paths and detached threads in wxBase. Prints the program's
// path, wxStandardPaths' directories, checks wxIsAbsolutePath on volume
// paths, and runs detached threads (which assert if they can't be made).
// Prints "pathtest: N failed".
// Copyright (c) 2026 AmiCradle Team
#include <wx/init.h>
#include <wx/string.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/thread.h>
#include <wx/log.h>
#include <wx/atomic.h>
#include <wx/utils.h>
#include <cstdio>

static int failed = 0;
static wxAtomicInt ran = 0;

static void Check(const char *what, bool ok)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    failed += !ok;
}

class Detached : public wxThread
{
public:
    Detached() : wxThread(wxTHREAD_DETACHED) {}
    ExitCode Entry() wxOVERRIDE { wxAtomicInc(ran); return 0; }
};

int main(int argc, char **argv)
{
    wxInitializer init(argc, argv);
    if (!init.IsOk()) {
        std::printf("pathtest: wx did not start\n");
        return 20;
    }
    // an assert in a thread or a path call shows here as a failure
    wxSetAssertHandler([](const wxString& file, int line, const wxString& func, const wxString& cond, const wxString& msg) {
        std::printf("FAIL assert %s:%d %s: %s %s\n", (const char *)file.mb_str(), line, (const char *)func.mb_str(),
                    (const char *)cond.mb_str(), (const char *)msg.mb_str());
        failed++;
    });
    wxStandardPaths& sp = wxStandardPaths::Get();
    const wxString exe = sp.GetExecutablePath();
    std::printf("exe:      %s\n", (const char *)exe.mb_str());
    std::printf("prefix:   %s\n", (const char *)sp.GetInstallPrefix().mb_str());
    std::printf("data:     %s\n", (const char *)sp.GetDataDir().mb_str());
    std::printf("userdata: %s\n", (const char *)sp.GetUserDataDir().mb_str());
    std::printf("config:   %s\n", (const char *)sp.GetUserConfigDir().mb_str());
    std::printf("cwd:      %s\n", (const char *)wxGetCwd().mb_str());
    Check("the program's path names a volume", exe.find(':') != wxString::npos && exe[0] != '/');
    Check("no path starts with /Volume:", sp.GetDataDir().find(':') == wxString::npos || sp.GetDataDir()[0] != '/');
    Check("wxIsAbsolutePath(System:CodeBlocks)", wxIsAbsolutePath("System:CodeBlocks"));
    Check("wxIsAbsolutePath(RAM:T/x)", wxIsAbsolutePath("RAM:T/x"));
    Check("!wxIsAbsolutePath(share/x)", !wxIsAbsolutePath("share/x"));
    Check("!wxIsAbsolutePath(a/b:c)", !wxIsAbsolutePath("a/b:c"));
    wxFileName fn("System:CodeBlocks/share/codeblocks/x.txt");
    fn.MakeAbsolute();
    Check("wxFileName keeps System:CodeBlocks/...", fn.GetFullPath() == "System:CodeBlocks/share/codeblocks/x.txt");
    for (int i = 0; i < 4; i++) {
        Detached *t = new Detached;
        Check("detached thread made", t->Run() == wxTHREAD_NO_ERROR);
    }
    for (int i = 0; i < 100 && ran < 4; i++)
        wxMilliSleep(20);
    Check("4 detached threads ran", ran == 4);
    wxMilliSleep(100);
    std::printf("pathtest: %d failed\n", failed);
    return failed ? 10 : 0;
}
