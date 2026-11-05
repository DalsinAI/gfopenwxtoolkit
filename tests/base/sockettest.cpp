// sockettest: wx's sockets (wxNet) on libnix and bsdsocket.library, on the
// loopback: an address lookup, a server and a client, data both ways, in
// blocking mode and then with wx's own waits. One line a check, and a last
// line "sockettest: N of M passed".
// Copyright (c) 2026 AmiCradle Team
#include <wx/init.h>
#include <wx/socket.h>
#include <wx/string.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int g_pass = 0, g_total = 0;

static void Check(bool ok, const char *what)
{
    g_total++;
    if ( ok )
        g_pass++;
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fflush(stdout);
}

// libnix's own calls, without wx: what wx's sockets stand on
static void RawChecks()
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    Check(srv >= 0, "raw: socket");
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    Check(bind(srv, (struct sockaddr *)&a, sizeof a) == 0, "raw: bind");
    Check(listen(srv, 4) == 0, "raw: listen");
    socklen_t len = sizeof a;
    getsockname(srv, (struct sockaddr *)&a, &len);
    printf("     raw port %u\n", (unsigned)ntohs(a.sin_port));
    int cli = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    Check(ioctl(cli, FIONBIO, &one) == 0, "raw: ioctl FIONBIO");
    int rc = connect(cli, (struct sockaddr *)&a, sizeof a);
    printf("     raw connect rc=%d errno=%d\n", rc, errno);
    fflush(stdout);
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(srv, &rd);
    struct timeval tv = { 3, 0 };
    rc = select(srv + 1, &rd, NULL, NULL, &tv);
    Check(rc == 1 && FD_ISSET(srv, &rd), "raw: select sees the connection");
    printf("     raw select rc=%d errno=%d\n", rc, errno);
    fflush(stdout);
    if ( rc == 1 )
    {
        int peer = accept(srv, NULL, NULL);
        Check(peer >= 0, "raw: accept");
        if ( peer >= 0 )
            close(peer);
    }
    close(cli);
    close(srv);
}

int main(int argc, char **argv)
{
    wxInitializer init(argc, argv);
    Check(init.IsOk(), "wx initialised");
    Check(wxSocketBase::Initialize(), "wxSocketBase::Initialize (bsdsocket.library open)");
    RawChecks();

    wxIPV4address any;
    Check(any.Hostname("127.0.0.1"), "address 127.0.0.1");
    wxIPV4address local;
    Check(local.Hostname("localhost"), "lookup localhost");
    printf("     localhost is %s\n", (const char *)local.IPAddress().mb_str());

    // a server on a port the stack picks
    wxIPV4address bindAddr;
    bindAddr.AnyAddress();
    bindAddr.Service(0);
    wxSocketServer server(bindAddr, wxSOCKET_REUSEADDR);
    Check(server.IsOk(), "server listens");
    wxIPV4address bound;
    server.GetLocal(bound);
    const unsigned short port = bound.Service();
    Check(port != 0, "server has a port");
    printf("     port %u\n", port);
    fflush(stdout);
    server.SetTimeout(5);

    // the client connects without waiting; the server accepts
    wxIPV4address to;
    to.Hostname("127.0.0.1");
    to.Service(port);
    wxSocketClient client(wxSOCKET_NONE);
    client.SetTimeout(5);
    client.Connect(to, false);
    printf("     connecting\n");
    fflush(stdout);
    wxSocketBase *peer = server.Accept(true);
    Check(peer != NULL, "server accepts");
    Check(client.WaitOnConnect(5, 0) && client.IsConnected(), "client connects");

    // client to server, 4096 bytes
    char out[4096], in[4096];
    for ( unsigned i = 0; i < sizeof out; i++ )
        out[i] = (char)(i * 7 + 3);
    client.SetFlags(wxSOCKET_WAITALL);
    client.Write(out, sizeof out);
    Check(!client.Error() && client.LastCount() == sizeof out, "client writes 4096 bytes");
    if ( peer )
    {
        peer->SetTimeout(5);
        peer->SetFlags(wxSOCKET_WAITALL);
        memset(in, 0, sizeof in);
        peer->Read(in, sizeof in);
        Check(peer->LastCount() == sizeof in && memcmp(in, out, sizeof in) == 0, "server reads the same 4096 bytes");

        // and back
        const char reply[] = "hello from the Amiga";
        peer->Write(reply, sizeof reply);
        char got[sizeof reply] = { 0 };
        client.Read(got, sizeof got);
        Check(client.LastCount() == sizeof reply && strcmp(got, reply) == 0, "client reads the reply");

        // the server closes: the client sees it
        peer->Destroy();
        char b;
        client.SetFlags(wxSOCKET_NONE);
        client.WaitForRead(5, 0);
        client.Read(&b, 1);
        Check(client.LastCount() == 0, "client sees the close");
    }
    client.Close();
    server.Close();

    // a datagram to ourselves
    wxIPV4address u1;
    u1.AnyAddress();
    u1.Service(0);
    wxDatagramSocket udp(u1, wxSOCKET_NONE);
    udp.SetTimeout(5);
    Check(udp.IsOk(), "datagram socket");
    wxIPV4address uLocal;
    udp.GetLocal(uLocal);
    wxIPV4address self;
    self.Hostname("127.0.0.1");
    self.Service(uLocal.Service());
    udp.SendTo(self, "ping", 5);
    char ub[8] = { 0 };
    udp.WaitForRead(5, 0);
    wxIPV4address from;
    udp.RecvFrom(from, ub, sizeof ub);
    Check(udp.LastCount() == 5 && strcmp(ub, "ping") == 0, "datagram comes back");

    wxSocketBase::Shutdown();
    printf("sockettest: %d of %d passed\n", g_pass, g_total);
    return g_pass == g_total ? 0 : 5;
}
