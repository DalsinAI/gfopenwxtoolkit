/* wxWebRequest on the GUI loop (libcurl, gfopentls.library for HTTPS):
 * fetches each address given, one after another, and prints its status,
 * size, content type and the first bytes, then ends.
 *   webtest http://example.com/ https://example.com/
 * With "file" before the addresses, each body goes to a temporary file
 * (wxWebRequest::Storage_File, as programs downloading big files use).
 * Writes the same to RAM:webtest.log. The run passes when it prints
 * "webtest: all N ok".
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/webrequest.h>
#include <wx/filename.h>
#include <stdio.h>

/* a GFOpenTLS handshake needs 16 KB of the task's stack, deep in curl */
unsigned long __stack = 131072;

static FILE *gs_log;

static void Say(const wxString& s)
{
    const wxScopedCharBuffer b = s.utf8_str();
    printf("webtest: %s\n", b.data());
    fflush(stdout);
    if ( gs_log )
    {
        fprintf(gs_log, "%s\n", b.data());
        fflush(gs_log);
    }
}

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        gs_log = fopen("RAM:webtest.log", "w");
        for ( int i = 1; i < argc; i++ )
        {
            const wxString a(argv[i]);
            if ( a == "file" )
                m_file = true;
            else
                m_urls.Add(a);
        }
        if ( m_urls.empty() )
        {
            m_urls.Add("http://example.com/");
            m_urls.Add("https://example.com/");
        }
        if ( !wxWebSession::IsBackendAvailable(wxWebSessionBackendCURL) )
        {
            Say("no curl backend");
            return false;
        }
        Bind(wxEVT_WEBREQUEST_STATE, &App::OnState, this);
        Bind(wxEVT_WEBREQUEST_DATA, &App::OnData, this);
        m_watchdog.SetOwner(this);
        Bind(wxEVT_TIMER, &App::OnWatchdog, this);
        Next();
        return true;
    }

    int OnExit() wxOVERRIDE
    {
        Say(wxString::Format("all %d %s", m_ok, m_ok == (int)m_urls.size() ? "ok" : "NOT ok"));
        if ( gs_log )
            fclose(gs_log);
        return wxApp::OnExit();
    }

private:
    void Next()
    {
        if ( m_at >= m_urls.size() )
        {
            ExitMainLoop();
            return;
        }
        const wxString url = m_urls[m_at];
        Say("get " + url);
        m_bytes = 0;
        m_started = wxGetLocalTimeMillis();
        m_req = wxWebSession::GetDefault().CreateRequest(this, url);
        if ( !m_req.IsOk() )
        {
            Say("  no request made");
            m_at++;
            Next();
            return;
        }
        if ( m_file )
            m_req.SetStorage(wxWebRequest::Storage_File);
        m_req.Start();
        m_watchdog.StartOnce(60000);
    }

    void OnData(wxWebRequestEvent& e)
    {
        m_bytes += e.GetDataSize();
    }

    void OnState(wxWebRequestEvent& e)
    {
        const wxString url = m_urls[m_at];
        const long ms = (wxGetLocalTimeMillis() - m_started).ToLong();
        switch ( e.GetState() )
        {
            case wxWebRequest::State_Completed:
            {
                m_watchdog.Stop();
                wxWebResponse r = e.GetResponse();
                Say(wxString::Format("  %d %s, %lld bytes, %s, %ld ms",
                    r.GetStatus(), r.GetStatusText(), (long long)r.GetContentLength(),
                    r.GetHeader("Content-Type"), ms));
                if ( m_file )
                {
                    const wxString f = e.GetDataFile();
                    Say(wxString::Format("  file %s, %s bytes", f, wxFileName::GetSize(f).ToString()));
                }
                else
                {
                    const wxString body = r.AsString();
                    Say(wxString::Format("  body %zu chars: %s", body.length(),
                        body.Left(60).Strip(wxString::both)));
                }
                if ( r.GetStatus() >= 200 && r.GetStatus() < 400 )
                    m_ok++;
                m_req = wxWebRequest();
                m_at++;
                CallAfter(&App::Next);
                break;
            }
            case wxWebRequest::State_Failed:
            case wxWebRequest::State_Unauthorized:
            case wxWebRequest::State_Cancelled:
                m_watchdog.Stop();
                Say(wxString::Format("  state %d: %s (%ld ms)", (int)e.GetState(), e.GetErrorDescription(), ms));
                m_req = wxWebRequest();
                m_at++;
                CallAfter(&App::Next);
                break;
            default:
                break;
        }
    }

    void OnWatchdog(wxTimerEvent&)
    {
        Say("  no answer in 60 s: cancelled");
        if ( m_req.IsOk() )
            m_req.Cancel();
    }

    wxArrayString m_urls;
    size_t m_at = 0;
    int m_ok = 0;
    bool m_file = false;
    long long m_bytes = 0;
    wxLongLong m_started;
    wxWebRequest m_req;
    wxTimer m_watchdog;
};

wxIMPLEMENT_APP(App);
