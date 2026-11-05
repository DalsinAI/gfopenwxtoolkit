/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/sockets.cpp
// Purpose:     wxSocket events in the GUI loop: the sockets wx watches are
//              waited on with bsdsocket.library's WaitSelect, together with
//              the windows' port, the timer and the wake signal
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

// one watch: a handler waiting for its socket to be readable or writable
struct Watch
{
    int id;
    int fd;
    wxFDIOHandler *handler;
    wxFDIOManager::Direction dir;
};

wxVector<Watch> gs_watches;
int gs_nextId = 1;

class wxOLFDIOManager : public wxFDIOManager
{
public:
    virtual int AddInput(wxFDIOHandler *handler, int fd, Direction d) wxOVERRIDE
    {
        Watch w;
        w.id = gs_nextId++;
        if ( gs_nextId <= 0 )
            gs_nextId = 1;
        w.fd = fd;
        w.handler = handler;
        w.dir = d;
        gs_watches.push_back(w);
        return w.id;
    }

    virtual void RemoveInput(wxFDIOHandler *WXUNUSED(handler), int id, Direction WXUNUSED(d)) wxOVERRIDE
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
};

} // anonymous namespace

wxFDIOManager *wxGUIAppTraits::GetFDIOManager()
{
    static wxOLFDIOManager s_manager;
    return &s_manager;
}

// Waits for the signals in mask, as Wait() does, and for the watched sockets
// at the same time; returns the signals that came. The sockets that became
// ready get their handlers called afterwards, from wxOLDispatchSockets().
static wxVector<int> gs_stackFds;   // per watch, during a wait
static wxVector<int> gs_ready;      // the watches found ready
static bool gs_anyReady = false;

ULONG wxOLWaitSignalsAndSockets(ULONG mask)
{
    gs_anyReady = false;
    if ( gs_watches.empty() || !SocketBase )
        return Wait(mask);

    // the stack's descriptor of each watch, for this wait
    fd_set rd, wr;
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    int nfds = 0;
    gs_stackFds.resize(gs_watches.size());
    for ( size_t i = 0; i < gs_watches.size(); i++ )
    {
        const Watch& w = gs_watches[i];
        const int s = StackSocket(w.fd);
        gs_stackFds[i] = s;
        if ( s < 0 || s >= FD_SETSIZE )
            continue;
        FD_SET(s, w.dir == wxFDIOManager::INPUT ? &rd : &wr);
        if ( s + 1 > nfds )
            nfds = s + 1;
    }
    if ( !nfds )
        return Wait(mask);

    const fd_set allRead = rd, allWrite = wr;
    ULONG sigs = mask;
    const long n = WaitSelect(nfds, &rd, &wr, NULL, NULL, &sigs);
    if ( n < 0 )
    {
        // a socket went bad: every handler tries its socket, and the one in
        // trouble finds the error on its read or write and closes it
        rd = allRead;
        wr = allWrite;
        sigs = SetSignal(0, mask) & mask;
    }
    if ( n != 0 )
    {
        // ready, by libnix descriptor: the handlers know those
        gs_ready.clear();
        for ( size_t i = 0; i < gs_watches.size(); i++ )
        {
            const int s = gs_stackFds[i];
            if ( s < 0 || s >= FD_SETSIZE )
                continue;
            if ( FD_ISSET(s, gs_watches[i].dir == wxFDIOManager::INPUT ? &rd : &wr) )
                gs_ready.push_back(gs_watches[i].id);
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
    // each ready one that is still there
    const wxVector<int> ready(gs_ready);
    for ( size_t r = 0; r < ready.size(); r++ )
    {
        for ( size_t i = 0; i < gs_watches.size(); i++ )
        {
            if ( gs_watches[i].id != ready[r] )
                continue;
            const Watch w = gs_watches[i];
            if ( w.dir == wxFDIOManager::INPUT )
                w.handler->OnReadWaiting();
            else
                w.handler->OnWriteWaiting();
            break;
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
