/* wxIPC (wxServer, wxClient, wxConnection) between two programs on this
 * Amiga, through a server name as Unix programs give it (a path), which
 * the port makes into a loopback port.
 *   Run >RAM:ipcserver.txt ipctest server
 *   ipctest client          (prints "ipctest: N of M passed")
 * The client's last Execute "quit" ends the server.
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/ipc.h>
#include <stdio.h>
#include <string.h>

static const char *const SERVICE = "/tmp/gfopenwx-ipctest";
static int g_pass = 0, g_total = 0;

static void Check(bool ok, const char *what)
{
    g_total++;
    if ( ok )
        g_pass++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fflush(stdout);
}

class ServerConnection : public wxConnection
{
public:
    bool OnExecute(const wxString& WXUNUSED(topic), const void *data, size_t size, wxIPCFormat WXUNUSED(format)) wxOVERRIDE
    {
        const wxString cmd((const char *)data, size);
        printf("server: execute '%s'\n", (const char *)cmd.mb_str());
        fflush(stdout);
        if ( cmd.StartsWith("quit") )
            wxTheApp->CallAfter([] { wxTheApp->ExitMainLoop(); });
        return true;
    }

    const void *OnRequest(const wxString& WXUNUSED(topic), const wxString& item, size_t *size, wxIPCFormat WXUNUSED(format)) wxOVERRIDE
    {
        m_reply = wxString("hello ") + item;
        m_buf = m_reply.utf8_str();
        *size = strlen(m_buf.data()) + 1;
        return m_buf.data();
    }

    bool OnPoke(const wxString& WXUNUSED(topic), const wxString& item, const void *data, size_t size, wxIPCFormat WXUNUSED(format)) wxOVERRIDE
    {
        printf("server: poke %s = %.*s\n", (const char *)item.mb_str(), (int)size, (const char *)data);
        fflush(stdout);
        return true;
    }

private:
    wxString m_reply;
    wxCharBuffer m_buf;
};

class Server : public wxServer
{
public:
    wxConnectionBase *OnAcceptConnection(const wxString& topic) wxOVERRIDE
    {
        printf("server: connection, topic '%s'\n", (const char *)topic.mb_str());
        fflush(stdout);
        return topic == "test" ? new ServerConnection : NULL;
    }
};

class App : public wxApp
{
public:
    App() : m_server(NULL), m_rc(1) { }

    bool OnInit() wxOVERRIDE
    {
        if ( argc > 1 && argv[1] == "server" )
        {
            m_server = new Server;
            if ( !m_server->Create(SERVICE) )
            {
                printf("server: could not start\n");
                return false;
            }
            printf("server: listening on %s\n", SERVICE);
            fflush(stdout);
            m_rc = 0;
            return true;    // the main loop runs until the client says quit
        }

        wxClient client;
        wxConnectionBase *c = NULL;
        // the server may still be starting
        for ( int i = 0; i < 20 && !c; i++ )
        {
            {
                wxLogNull quiet;
                c = client.MakeConnection("", SERVICE, "test");
            }
            if ( !c )
                wxMilliSleep(250);
        }
        Check(c != NULL, "client connects (topic test)");
        if ( c )
        {
            size_t size = 0;
            const void *r = c->Request("world", &size);
            Check(r && size > 0 && strcmp((const char *)r, "hello world") == 0, "Request: \"hello world\"");
            Check(c->Poke("item", "value"), "Poke");
            Check(c->Execute("quit"), "Execute quit");
            c->Disconnect();
            delete c;
        }
        printf("ipctest: %d of %d passed\n", g_pass, g_total);
        m_rc = g_pass == g_total ? 0 : 5;
        return false;
    }

    int OnExit() wxOVERRIDE
    {
        delete m_server;
        m_server = NULL;
        return m_rc;
    }

    int OnRun() wxOVERRIDE
    {
        wxApp::OnRun();
        return m_rc;
    }

private:
    Server *m_server;
    int m_rc;
};

wxIMPLEMENT_APP(App);
