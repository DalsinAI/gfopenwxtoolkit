/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/app.cpp
// Purpose:     wxApp, wxGUIAppTraits and wxGUIEventLoop: every window's
//              Intuition messages on one port, wx's timers on timer.device,
//              and a signal other tasks wake the loop with
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/nonownedwnd.h"
    #include "wx/toplevel.h"
#endif

#include "wx/apptrait.h"
#include "wx/evtloop.h"
#include "wx/unix/private/timer.h"
#include "wx/private/eventloopsourcesmanager.h"

#include <exec/ports.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include "private.h"

void wxOLFreeMasterUI();
void wxOLCloseKeymap();
void wxOLCloseGadTools();
size_t wxOLOpenWindowCount();
ULONG wxOLWaitSignalsAndSockets(ULONG mask);
bool wxOLDispatchSockets();
wxNonOwnedWindow *wxOLOpenWindow(size_t i);

// ---- the port, the wake signal, the timer -------------------------------------------

static struct MsgPort *gs_port = NULL;

static struct Task *gs_mainTask = NULL;
static LONG gs_wakeSig = -1;
static struct MsgPort *gs_timerPort = NULL;
static struct timerequest *gs_timerReq = NULL;
static bool gs_timerOpen = false;
static bool gs_timerPending = false;

struct MsgPort *wxOLSharedPort()
{
    if ( !gs_port )
        gs_port = CreateMsgPort();
    return gs_port;
}

void wxOLFreeSharedPort()
{
    if ( gs_port )
    {
        struct Message *m;
        while ( (m = GetMsg(gs_port)) != NULL )
            ReplyMsg(m);
        DeleteMsgPort(gs_port);
    }
    gs_port = NULL;
}

unsigned long wxOLWakeSignal()
{
    if ( gs_wakeSig < 0 )
    {
        gs_mainTask = FindTask(NULL);
        gs_wakeSig = AllocSignal(-1);
    }
    return gs_wakeSig >= 0 ? 1UL << gs_wakeSig : 0;
}

void wxOLWakeMain()
{
    if ( gs_mainTask && gs_wakeSig >= 0 )
        Signal(gs_mainTask, 1UL << gs_wakeSig);
}

static bool OpenTimer()
{
    if ( gs_timerOpen )
        return true;
    if ( !(gs_timerPort = CreateMsgPort()) )
        return false;
    gs_timerReq = (struct timerequest *)CreateIORequest(gs_timerPort, sizeof(struct timerequest));
    if ( !gs_timerReq || OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, &gs_timerReq->tr_node, 0) != 0 )
    {
        if ( gs_timerReq )
            DeleteIORequest(&gs_timerReq->tr_node);
        DeleteMsgPort(gs_timerPort);
        gs_timerReq = NULL;
        gs_timerPort = NULL;
        return false;
    }
    gs_timerOpen = true;
    return true;
}

static void StopTimer()
{
    if ( gs_timerPending )
    {
        if ( !CheckIO(&gs_timerReq->tr_node) )
            AbortIO(&gs_timerReq->tr_node);
        WaitIO(&gs_timerReq->tr_node);
        gs_timerPending = false;
    }
    SetSignal(0, 1UL << gs_timerPort->mp_SigBit);
}

static void CloseTimer()
{
    if ( !gs_timerOpen )
        return;
    StopTimer();
    CloseDevice(&gs_timerReq->tr_node);
    DeleteIORequest(&gs_timerReq->tr_node);
    DeleteMsgPort(gs_timerPort);
    gs_timerReq = NULL;
    gs_timerPort = NULL;
    gs_timerOpen = false;
}

// ---- the messages ----------------------------------------------------------------------

// one message, copied and replied before it is handled: the window it is for
// may close (and its port go) while wx handles it
static bool HandleOneMessage()
{
    if ( !gs_port )
        return false;
    struct IntuiMessage *m = (struct IntuiMessage *)GetMsg(gs_port);
    if ( !m )
        return false;

    // a run of pointer moves for one window: the last says where it is
    if ( m->Class == IDCMP_MOUSEMOVE )
    {
        struct IntuiMessage *next = (struct IntuiMessage *)gs_port->mp_MsgList.lh_Head;
        if ( next->ExecMessage.mn_Node.ln_Succ && next->Class == IDCMP_MOUSEMOVE && next->IDCMPWindow == m->IDCMPWindow )
        {
            ReplyMsg(&m->ExecMessage);
            return true;
        }
    }

    struct IntuiMessage copy = *m;
    ULONG prev = 0;
    if ( m->Class == IDCMP_RAWKEY && m->IAddress )
    {
        // the dead keys before it, for the keymap
        prev = *(ULONG *)m->IAddress;
        copy.IAddress = &prev;
    }
    struct Window *win = m->IDCMPWindow;
    ReplyMsg(&m->ExecMessage);

    size_t i;
    for ( i = 0; i < wxOLOpenWindowCount(); i++ )
    {
        wxNonOwnedWindow *w = wxOLOpenWindow(i);
        if ( w->OLGetWindow() == win )
        {
            w->OLHandleMessage(&copy);
            break;
        }
    }
    return true;
}

// what changed in every window is drawn: before the loop waits
static void DrawAll()
{
    for ( size_t i = 0; i < wxOLOpenWindowCount(); i++ )
        wxOLOpenWindow(i)->OLDrawPending();
}

static bool PortHasMessages()
{
    return gs_port && gs_port->mp_MsgList.lh_Head->ln_Succ != NULL;
}

static bool TimerDue()
{
#if wxUSE_TIMER
    wxUsecClock_t remaining;
    return wxTimerScheduler::Get().GetNext(&remaining) && remaining == 0;
#else
    return false;
#endif
}

// ---- wxGUIEventLoop -------------------------------------------------------------------------

wxGUIEventLoop::wxGUIEventLoop()
{
    wxOLWakeSignal();
}

bool wxGUIEventLoop::Pending() const
{
    if ( PortHasMessages() || TimerDue() )
        return true;
    const ULONG wake = wxOLWakeSignal();
    return wake && (SetSignal(0, 0) & wake);
}

bool wxGUIEventLoop::DoDispatch(long timeout)
{
    // what is waiting already
    if ( HandleOneMessage() )
        return true;
#if wxUSE_TIMER
    if ( wxTimerScheduler::Get().NotifyExpired() )
        return true;
#endif
    const ULONG wake = wxOLWakeSignal();
    if ( wake && (SetSignal(0, wake) & wake) )
        return true;

    DrawAll();
    if ( PortHasMessages() )
        return HandleOneMessage();

    // the time to the next timer, when it is sooner
    long wait = timeout;
#if wxUSE_TIMER
    wxUsecClock_t remaining;
    if ( wxTimerScheduler::Get().GetNext(&remaining) )
    {
        const long ms = ((remaining + 999) / 1000).ToLong();
        if ( wait < 0 || ms < wait )
            wait = ms;
    }
#endif
    if ( wait == 0 )
    {
#if wxUSE_TIMER
        return wxTimerScheduler::Get().NotifyExpired();
#else
        return false;
#endif
    }

    ULONG mask = wake | SIGBREAKF_CTRL_C;
    if ( gs_port )
        mask |= 1UL << gs_port->mp_SigBit;
    if ( wait > 0 && OpenTimer() )
    {
        gs_timerReq->tr_node.io_Command = TR_ADDREQUEST;
        gs_timerReq->tr_time.tv_secs = (ULONG)(wait / 1000);
        gs_timerReq->tr_time.tv_micro = (ULONG)((wait % 1000) * 1000);
        SetSignal(0, 1UL << gs_timerPort->mp_SigBit);
        SendIO(&gs_timerReq->tr_node);
        gs_timerPending = true;
        mask |= 1UL << gs_timerPort->mp_SigBit;
    }
    // the windows' signals, and the sockets wx watches (sockets.cpp)
    const ULONG got = wxOLWaitSignalsAndSockets(mask);
    if ( gs_timerOpen )
        StopTimer();

    if ( got & SIGBREAKF_CTRL_C )
    {
        // Ctrl-C from the Shell: the program ends as if its last window closed
        if ( wxTheApp )
            wxTheApp->ExitMainLoop();
        return true;
    }
    bool handled = (got & wake) != 0;
    if ( wxOLDispatchSockets() )
        handled = true;
    if ( HandleOneMessage() )
        handled = true;
#if wxUSE_TIMER
    if ( wxTimerScheduler::Get().NotifyExpired() )
        handled = true;
#endif
    return handled;
}

bool wxGUIEventLoop::Dispatch()
{
    DoDispatch(-1);
    return true;
}

int wxGUIEventLoop::DispatchTimeout(unsigned long timeout)
{
    if ( m_shouldExit )
        return -1;
    return DoDispatch((long)timeout) ? 1 : 0;
}

void wxGUIEventLoop::WakeUp()
{
    wxOLWakeMain();
}

void wxGUIEventLoop::DoYieldFor(long eventsToProcess)
{
    // the messages waiting now, and the timers due; nothing is waited for
    int n = 0;
    while ( n++ < 256 && HandleOneMessage() )
        ;
#if wxUSE_TIMER
    wxTimerScheduler::Get().NotifyExpired();
#endif
    DrawAll();
    wxEventLoopBase::DoYieldFor(eventsToProcess);
}

// ---- wxGUIAppTraits ---------------------------------------------------------------------------

wxEventLoopBase *wxGUIAppTraits::CreateEventLoop()
{
    return new wxGUIEventLoop;
}

int wxGUIAppTraits::WaitForChild(wxExecuteData& WXUNUSED(execData))
{
    // wxExecute(wxEXEC_SYNC) of a child process: not on AmigaOS (no fork)
    return -1;
}

#if wxUSE_TIMER
wxTimerImpl *wxGUIAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxUnixTimerImpl(timer);
}
#endif

wxPortId wxGUIAppTraits::GetToolkitVersion(int *majVer, int *minVer, int *microVer) const
{
    // GFOpenLayout 0.5
    if ( majVer ) *majVer = 0;
    if ( minVer ) *minVer = 5;
    if ( microVer ) *microVer = 0;
    return wxPORT_BASE;
}

#if wxUSE_EVENTLOOP_SOURCE
wxEventLoopSourcesManagerBase *wxGUIAppTraits::GetEventLoopSourcesManager()
{
    // file descriptors are the console's: the GUI loop does not watch them
    return wxAppTraits::GetEventLoopSourcesManager();
}
#endif

// ---- wxApp ---------------------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxApp, wxEvtHandler);

wxApp::wxApp()
{
}

wxApp::~wxApp()
{
}

bool wxApp::Initialize(int& argc, wxChar **argv)
{
    if ( !wxAppBase::Initialize(argc, argv) )
        return false;
    wxOLWakeSignal();
    if ( !wxOLGetScreen() )
    {
        wxLogError("There is no screen to open windows on.");
        return false;
    }
    if ( !wxOLSharedPort() )
        return false;
    return true;
}

void wxApp::CleanUp()
{
    wxAppBase::CleanUp();
    // the windows are gone: then what they shared
    wxOLFreeSharedPort();
    CloseTimer();
    wxOLFreeMasterUI();
    wxOLCloseKeymap();
#if wxUSE_MENUS
    wxOLCloseGadTools();
#endif
    wxOLReleasePens();
    wxOLReleaseScreen();
    if ( gs_wakeSig >= 0 )
        FreeSignal(gs_wakeSig);
    gs_wakeSig = -1;
}

void wxApp::WakeUpIdle()
{
    wxOLWakeMain();
}
