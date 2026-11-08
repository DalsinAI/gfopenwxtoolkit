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
#include <wx/filesys.h>
#include <wx/uilocale.h>
#include <wx/fs_arc.h>
#include <wx/zipstrm.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
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
    {
        // a volume alone as a folder: what is in it follows the colon
        wxFileName t;
        t.AssignDir("T:");
        Check("AssignDir(T:) + name is T:wxd", wxFileName(t.GetPath(), "wxd").GetFullPath() == "T:wxd");
        Check("AssignDir(T:) full path is T:", t.GetFullPath() == "T:");
        Check("wxFileName(RAM:, x) is RAM:x", wxFileName("RAM:", "x").GetFullPath() == "RAM:x");
        wxFileName d;
        d.AssignDir("RAM:T");
        Check("AssignDir(RAM:T) full path is RAM:T/", d.GetFullPath() == "RAM:T/");
    }
    // wxFileSystem on volume paths, as Code::Blocks opens its resources
    {
        wxFileSystem::AddHandler(new wxArchiveFSHandler);
        {
            wxFFileOutputStream fo("RAM:T/fstest.zip");
            wxZipOutputStream zip(fo);
            zip.PutNextEntry("a.txt");
            zip.Write("hello", 5);
        }
        std::printf("url:      %s\n", (const char *)wxFileSystem::FileNameToURL(wxFileName("RAM:T/fstest.zip")).mb_str());
        wxFileSystem fs;
        wxFSFile *f = fs.OpenFile("RAM:T/fstest.zip");
        Check("wxFileSystem opens RAM:T/fstest.zip", f != NULL);
        delete f;
        f = fs.OpenFile("RAM:T/fstest.zip#zip:a.txt");
        char buf[8] = {0};
        if (f)
            f->GetStream()->Read(buf, 5);
        Check("wxFileSystem reads RAM:T/fstest.zip#zip:a.txt", f && std::string(buf) == "hello");
        delete f;
        fs.ChangePathTo("RAM:T/fstest.zip#zip:", true);
        f = fs.OpenFile("a.txt");
        Check("wxFileSystem reads a.txt after ChangePathTo(RAM:T/fstest.zip#zip:)", f != NULL);
        delete f;
        wxString found = wxFileSystem().FindFirst("RAM:T/fstest.zip#zip:*.txt");
        std::printf("found:    %s\n", (const char *)found.mb_str());
        Check("wxFileSystem finds RAM:T/fstest.zip#zip:*.txt", !found.empty());
        wxRemoveFile("RAM:T/fstest.zip");
    }
    // numbers and dates: libnix's locale leaves these empty
    {
        const wxString dp = wxUILocale::GetCurrent().GetInfo(wxLOCALE_DECIMAL_POINT);
        std::printf("decimal:  '%s', date '%s', time '%s'\n", (const char *)dp.mb_str(),
                    (const char *)wxUILocale::GetCurrent().GetInfo(wxLOCALE_SHORT_DATE_FMT).mb_str(),
                    (const char *)wxUILocale::GetCurrent().GetInfo(wxLOCALE_TIME_FMT).mb_str());
        Check("a decimal point", !dp.empty());
        Check("wxString::FromCDouble(1.5) is 1.5", wxString::FromCDouble(1.5) == "1.5");
        Check("wxString::FromDouble(2.25, 2) has 4 characters", wxString::FromDouble(2.25, 2).length() == 4);
    }
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
