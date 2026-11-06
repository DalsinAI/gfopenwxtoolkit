// basetest: wxBase checks on AmigaOS (strings, files, config, dates, threads).
// Prints one line per check and "basetest: N passed, M failed".
// Copyright (c) 2026 AmiCradle Team
#include <wx/init.h>
#include <wx/string.h>
#include <wx/filename.h>
#include <wx/fileconf.h>
#include <wx/file.h>
#include <wx/wfstream.h>
#include <wx/datetime.h>
#include <wx/thread.h>
#include <wx/tokenzr.h>
#include <wx/regex.h>
#include <wx/textfile.h>
#include <wx/dir.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <wx/log.h>
#include <wx/xml/xml.h>
#include <wx/sstream.h>
#include <cstdio>

static int pass, fail;
static void check(bool ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    ok ? ++pass : ++fail;
}

class Counter : public wxThread
{
public:
    Counter(wxMutex &m, int &n) : wxThread(wxTHREAD_JOINABLE), m_m(m), m_n(n) {}
    ExitCode Entry() override
    {
        for (int i = 0; i < 1000; i++) { wxMutexLocker l(m_m); m_n++; }
        return 0;
    }
private:
    wxMutex &m_m; int &m_n;
};

int main(int argc, char **argv)
{
    // each line as it is made, so a hang shows where it stopped
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("     starting wx\n");
    wxInitializer init(argc, argv);
    check(init.IsOk(), "wxInitializer");

    wxString s = wxString::Format("%d-%s-%.2f", 42, "amiga", 3.25);
    check(s == "42-amiga-3.25", "wxString::Format");
    check(wxString::FromUTF8("\xc3\xa9t\xc3\xa9").length() == 3, "UTF-8 to wxString");
    check(wxString(L"été").utf8_str().length() == 5, "wxString to UTF-8");
    wxStringTokenizer tk("a,b,,c", ",", wxTOKEN_RET_EMPTY);
    check(tk.CountTokens() == 4, "wxStringTokenizer");
    check(s.Upper() == "42-AMIGA-3.25", "Upper");

    wxRegEx re("([a-z]+)-([0-9.]+)");
    check(re.IsValid() && re.Matches(s) && re.GetMatch(s, 2) == "3.25", "wxRegEx");
    {
        // regex.library: case, replace, wxRE_ADVANCED (its Perl syntax), UTF-8
        wxRegEx ic("AMIGA", wxRE_ICASE);
        check(ic.IsValid() && ic.Matches(s), "wxRegEx wxRE_ICASE");
        wxString r = "a1b22c333";
        wxRegEx digits("[0-9]+");
        check(digits.Replace(&r, "#") == 3 && r == "a#b#c#", "wxRegEx::Replace");
        wxRegEx adv("\\d+\\.(\\d+)", wxRE_ADVANCED);
        check(adv.IsValid() && adv.Matches(s) && adv.GetMatch(s, 1) == "25", "wxRegEx wxRE_ADVANCED");
        wxRegEx u("t(.)t");
        wxString e = wxString::FromUTF8("\xc3\xa9t\xc3\xa9t\xc3\xa9");
        check(u.IsValid() && u.Matches(e) && u.GetMatch(e, 1) == wxString::FromUTF8("\xc3\xa9"), "wxRegEx UTF-8");
        wxLogNull quiet;
        wxRegEx bad("(unclosed");
        check(!bad.IsValid(), "wxRegEx bad pattern refused");
    }
    {
        // OXP behind wxXmlDocument
        wxStringInputStream in(wxString::FromUTF8(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<resource version=\"2.3.0.1\">\n"
            "  <!-- a comment -->\n"
            "  <object class=\"wxFrame\" name=\"main\"><title>Caf\xc3\xa9 &amp; co</title>"
            "<![CDATA[<raw>]]></object>\n"
            "</resource>\n"));
        wxXmlDocument doc;
        bool ok = doc.Load(in);
        wxXmlNode *root = ok ? doc.GetRoot() : NULL;
        wxXmlNode *obj = root ? root->GetChildren() : NULL;
        while (obj && obj->GetType() != wxXML_ELEMENT_NODE) obj = obj->GetNext();
        wxXmlNode *title = obj ? obj->GetChildren() : NULL;
        check(ok && root && root->GetName() == "resource" && root->GetAttribute("version") == "2.3.0.1"
              && doc.GetVersion() == "1.0" && doc.GetFileEncoding() == "UTF-8", "wxXmlDocument load");
        check(obj && obj->GetAttribute("name") == "main" && title && title->GetName() == "title"
              && title->GetNodeContent() == wxString::FromUTF8("Caf\xc3\xa9 & co")
              && title->GetNext() && title->GetNext()->GetType() == wxXML_CDATA_SECTION_NODE
              && title->GetNext()->GetContent() == "<raw>", "wxXmlDocument nodes");
        wxLogNull quiet;
        wxStringInputStream broken("<a><b></a>");
        wxXmlDocument bad;
        check(!bad.Load(broken), "wxXmlDocument refuses bad XML");
    }

    wxFileName fn("RAM:T/wxtest/sub/file.txt");
    check(fn.GetName() == "file" && fn.GetExt() == "txt", "wxFileName name/ext");
    printf("     path: %s volume: %s\n", (const char *)fn.GetPath().utf8_str(), (const char *)fn.GetVolume().utf8_str());
    check(wxFileName::Mkdir("RAM:T/wxtest/sub", wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL), "Mkdir full");
    {
        wxTextFile tf(fn.GetFullPath());
        tf.Create();
        tf.AddLine("one"); tf.AddLine("two");
        check(tf.Write(), "wxTextFile write");
    }
    {
        wxTextFile tf(fn.GetFullPath());
        check(tf.Open() && tf.GetLineCount() == 2 && tf[1] == "two", "wxTextFile read back");
    }
    check(wxFileExists("RAM:T/wxtest/sub/file.txt"), "wxFileExists");
    wxArrayString files;
    check(wxDir::GetAllFiles("RAM:T/wxtest", &files) == 1, "wxDir::GetAllFiles");

    {
        wxFileConfig cfg("basetest", "", "RAM:T/wxtest/basetest.cfg", "", wxCONFIG_USE_LOCAL_FILE);
        cfg.Write("/Audio/Rate", 44100L);
        cfg.Write("/Audio/Name", "paula");
        cfg.Flush();
    }
    {
        wxFileConfig cfg("basetest", "", "RAM:T/wxtest/basetest.cfg", "", wxCONFIG_USE_LOCAL_FILE);
        long rate = cfg.ReadLong("/Audio/Rate", 0);
        wxString name = cfg.Read("/Audio/Name", "");
        if (rate != 44100 || name != "paula") {
            wxString text;
            wxFile f;
            if (f.Open("RAM:T/wxtest/basetest.cfg"))
                f.ReadAll(&text);
            printf("     cfg: exists=%d rate=%ld name='%s' file='%s'\n", (int)wxFileExists("RAM:T/wxtest/basetest.cfg"),
                   rate, (const char *)name.utf8_str(), (const char *)text.utf8_str());
        }
        check(rate == 44100 && name == "paula", "wxFileConfig round trip");
    }

    wxDateTime d(4, wxDateTime::Nov, 2026, 18, 9, 52);
    check(d.Format("%Y-%m-%d %H:%M:%S") == "2026-11-04 18:09:52", "wxDateTime::Format");
    check(wxDateTime::Now().GetYear() >= 2024, "wxDateTime::Now");
    wxLongLong t0 = wxGetUTCTimeMillis();
    wxMilliSleep(100);
    long ms = (wxGetUTCTimeMillis() - t0).ToLong();
    printf("     slept %ld ms\n", ms);
    check(ms >= 80 && ms < 1000, "wxMilliSleep 100 ms");

    wxMutex m; int n = 0;
    Counter a(m, n), b(m, n);
    check(a.Run() == wxTHREAD_NO_ERROR && b.Run() == wxTHREAD_NO_ERROR, "two wxThreads run");
    a.Wait(); b.Wait();
    check(n == 2000, "wxMutex counts 2000");

    printf("     user dir: %s\n", (const char *)wxStandardPaths::Get().GetUserDataDir().utf8_str());
    wxFileName::Rmdir("RAM:T/wxtest", wxPATH_RMDIR_RECURSIVE);
    check(!wxDirExists("RAM:T/wxtest"), "Rmdir recursive");

    printf("basetest: %d passed, %d failed\n", pass, fail);
    return fail ? 10 : 0;
}
