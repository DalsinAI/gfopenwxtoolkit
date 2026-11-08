/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/sockets.cpp
// Purpose:     wxSocket events in the GUI loop: the sockets wx watches are
//              waited on with bsdsocket.library's WaitSelect, together with
//              the windows' port, the timer and the wake signal; and the
//              event loop sources wxWebRequest's curl watches its sockets by
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_SOCKETS

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#include "wx/apptrait.h"
#include "wx/vector.h"
#include "wx/private/fdiomanager.h"
#include "wx/private/fdiohandler.h"
#if wxUSE_EVENTLOOP_SOURCE
    #include "wx/evtloopsrc.h"
    #include "wx/private/eventloopsourcesmanager.h"
#endif

#include <exec/types.h>
#include <clib/exec_protos.h>
#include <stdio.h>          // libnix's StdFileDes: its socket descriptors
#include <sys/socket.h>
#include <proto/bsdsocket.h>  // WaitSelect, on libnix's own SocketBase

#include "private.h"

// libnix (-lsocket) opens bsdsocket.library for the main task and gives each
// socket a descriptor of its own; WaitSelect wants the stack's numbers.
static int StackSocket(int fd)
{
    StdFileDes * const sfd = _lx_fhfromfd(fd);
    if ( !sfd || !(sfd->lx_flags & LX_SOCKET) )
        return -1;
    return sfd->lx_sock;
}

namespace
{

// one watch: a handler waiting for its socket to be readable or writable;
// or an event loop source (src set), whose fd is the stack's own socket
// number (curl's), waited on for the wxEVENT_SOURCE_ flags it has
struct Watch
{
    int id;
    int fd;
    wxFDIOHandler *handler;
    wxFDIOManager::Direction dir;
#if wxUSE_EVENTLOOP_SOURCE
    wxEventLoopSourceHandler *src;
    int flags;
#endif
};

// what a wait found ready on a watch
enum { READY_IN = 1, READY_OUT = 2, READY_EX = 4 };
struct Ready
{
    int id;
    int what;
};

wxVector<Watch> gs_watches;
int gs_nextId = 1;

int AddWatch(Watch& w)
{
    w.id = gs_nextId++;
    if ( gs_nextId <= 0 )
        gs_nextId = 1;
    gs_watches.push_back(w);
    return w.id;
}

void RemoveWatch(int id)
{
    for ( size_t i = 0; i < gs_watches.size(); i++ )
    {
        if ( gs_watches[i].id == id )
        {
            gs_watches.erase(gs_watches.begin() + i);
            return;
        }
    }
}

const Watch *FindWatch(int id)
{
    for ( size_t i = 0; i < gs_watches.size(); i++ )
        if ( gs_watches[i].id == id )
            return &gs_watches[i];
    return NULL;
}

class wxOLFDIOManager : public wxFDIOManager
{
public:
    virtual int AddInput(wxFDIOHandler *handler, int fd, Direction d) wxOVERRIDE
    {
        Watch w;
        w.fd = fd;
        w.handler = handler;
        w.dir = d;
#if wxUSE_EVENTLOOP_SOURCE
        w.src = NULL;
        w.flags = 0;
#endif
        return AddWatch(w);
    }

    virtual void RemoveInput(wxFDIOHandler *WXUNUSED(handler), int id, Direction WXUNUSED(d)) wxOVERRIDE
    {
        RemoveWatch(id);
    }
};

#if wxUSE_EVENTLOOP_SOURCE

// A source the GUI loop watches: deleting it ends the watch (curl's
// poller deletes it and makes a new one when what it waits for changes)
class wxOLEventLoopSource : public wxEventLoopSource
{
public:
    wxOLEventLoopSource(wxEventLoopSourceHandler *handler, int flags, int id)
        : wxEventLoopSource(handler, flags), m_id(id) { }
    virtual ~wxOLEventLoopSource() { RemoveWatch(m_id); }

private:
    int m_id;
};

// The GUI loop's sources: a descriptor given here is a socket of
// bsdsocket.library's own numbering on the C library's SocketBase, as curl
// (on <proto/bsdsocket.h>) has them, not one of libnix's descriptors
class wxOLEventLoopSourcesManager : public wxEventLoopSourcesManagerBase
{
public:
    virtual wxEventLoopSource *AddSourceForFD(int fd, wxEventLoopSourceHandler *handler, int flags) wxOVERRIDE
    {
        wxCHECK_MSG( fd >= 0 && fd < FD_SETSIZE, NULL, "can't watch this socket" );
        wxCHECK_MSG( handler, NULL, "a source needs a handler" );
        if ( !SocketBase )
            return NULL;
        Watch w;
        w.fd = fd;
        w.handler = NULL;
        w.dir = wxFDIOManager::INPUT;
        w.src = handler;
        w.flags = flags;
        return new wxOLEventLoopSource(handler, flags, AddWatch(w));
    }
};

#endif // wxUSE_EVENTLOOP_SOURCE

} // anonymous namespace

#if wxUSE_EVENTLOOP_SOURCE
wxEventLoopSourcesManagerBase *wxOLGetEventLoopSourcesManager()
{
    static wxOLEventLoopSourcesManager s_manager;
    return &s_manager;
}
#endif

wxFDIOManager *wxGUIAppTraits::GetFDIOManager()
{
    static wxOLFDIOManager s_manager;
    return &s_manager;
}

// Waits for the signals in mask, as Wait() does, and for the watched sockets
// at the same time; returns the signals that came. The sockets that became
// ready get their handlers called afterwards, from wxOLDispatchSockets().
static wxVector<int> gs_stackFds;   // per watch, during a wait
static wxVector<Ready> gs_ready;    // the watches found ready
static bool gs_anyReady = false;

ULONG wxOLWaitSignalsAndSockets(ULONG mask)
{
    gs_anyReady = false;
    if ( gs_watches.empty() || !SocketBase )
        return Wait(mask);

    // the stack's descriptor of each watch, for this wait
    fd_set rd, wr, ex;
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);
    int nfds = 0;
    gs_stackFds.resize(gs_watches.size());
    for ( size_t i = 0; i < gs_watches.size(); i++ )
    {
        const Watch& w = gs_watches[i];
        int what = w.dir == wxFDIOManager::INPUT ? READY_IN : READY_OUT;
        int s;
#if wxUSE_EVENTLOOP_SOURCE
        if ( w.src )
        {
            // a source's socket is the stack's number already
            s = w.fd;
            what = (w.flags & wxEVENT_SOURCE_INPUT ? READY_IN : 0)
                 | (w.flags & wxEVENT_SOURCE_OUTPUT ? READY_OUT : 0)
                 | (w.flags & wxEVENT_SOURCE_EXCEPTION ? READY_EX : 0);
        }
        else
#endif
            s = StackSocket(w.fd);
        gs_stackFds[i] = s;
        if ( s < 0 || s >= FD_SETSIZE )
            continue;
        if ( what & READY_IN )
            FD_SET(s, &rd);
        if ( what & READY_OUT )
            FD_SET(s, &wr);
        if ( what & READY_EX )
            FD_SET(s, &ex);
        if ( s + 1 > nfds )
            nfds = s + 1;
    }
    if ( !nfds )
        return Wait(mask);

    const fd_set allRead = rd, allWrite = wr;
    ULONG sigs = mask;
    const long n = WaitSelect(nfds, &rd, &wr, &ex, NULL, &sigs);
    if ( n < 0 )
    {
        // a socket went bad: every handler tries its socket, and the one in
        // trouble finds the error on its read or write and closes it
        rd = allRead;
        wr = allWrite;
        FD_ZERO(&ex);
        sigs = SetSignal(0, mask) & mask;
    }
    if ( n != 0 )
    {
        // ready, by watch: the handlers know their own descriptors
        gs_ready.clear();
        for ( size_t i = 0; i < gs_watches.size(); i++ )
        {
            const int s = gs_stackFds[i];
            if ( s < 0 || s >= FD_SETSIZE )
                continue;
            const Watch& w = gs_watches[i];
            Ready r;
            r.id = w.id;
            r.what = 0;
#if wxUSE_EVENTLOOP_SOURCE
            if ( w.src )
            {
                if ( (w.flags & wxEVENT_SOURCE_INPUT) && FD_ISSET(s, &rd) )
                    r.what |= READY_IN;
                if ( (w.flags & wxEVENT_SOURCE_OUTPUT) && FD_ISSET(s, &wr) )
                    r.what |= READY_OUT;
                if ( (w.flags & wxEVENT_SOURCE_EXCEPTION) && FD_ISSET(s, &ex) )
                    r.what |= READY_EX;
            }
            else
#endif
            if ( FD_ISSET(s, w.dir == wxFDIOManager::INPUT ? &rd : &wr) )
                r.what = w.dir == wxFDIOManager::INPUT ? READY_IN : READY_OUT;
            if ( r.what )
                gs_ready.push_back(r);
        }
        gs_anyReady = !gs_ready.empty();
    }
    return sigs;
}

// Calls the handlers of the sockets the last wait found ready; true when
// there were any.
bool wxOLDispatchSockets()
{
    if ( !gs_anyReady )
        return false;
    gs_anyReady = false;

    // a handler can add and remove watches (its own or others'): call
    // each ready one that is still there, again before each of its calls
    const wxVector<Ready> ready(gs_ready);
    for ( size_t r = 0; r < ready.size(); r++ )
    {
        static const int order[] = { READY_EX, READY_IN, READY_OUT };
        for ( size_t k = 0; k < WXSIZEOF(order); k++ )
        {
            if ( !(ready[r].what & order[k]) )
                continue;
            const Watch *p = FindWatch(ready[r].id);
            if ( !p )
                break;
            const Watch w = *p;
#if wxUSE_EVENTLOOP_SOURCE
            if ( w.src )
            {
                if ( order[k] == READY_EX )
                    w.src->OnExceptionWaiting();
                else if ( order[k] == READY_IN )
                    w.src->OnReadWaiting();
                else
                    w.src->OnWriteWaiting();
                continue;
            }
#endif
            if ( order[k] == READY_IN )
                w.handler->OnReadWaiting();
            else if ( order[k] == READY_OUT )
                w.handler->OnWriteWaiting();
        }
    }
    return true;
}

#else // !wxUSE_SOCKETS

#include <exec/types.h>
#include <clib/exec_protos.h>

ULONG wxOLWaitSignalsAndSockets(ULONG mask)
{
    return Wait(mask);
}

bool wxOLDispatchSockets()
{
    return false;
}

#endif // wxUSE_SOCKETS
