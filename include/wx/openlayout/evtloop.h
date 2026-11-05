/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/evtloop.h
// Purpose:     the GUI event loop: Intuition windows, timers, wakeups
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_EVTLOOP_H_
#define _WX_OPENLAYOUT_EVTLOOP_H_

// The loop waits on one message port shared by every window the port opens
// (IDCMP), a timer.device request for the next wx timer, and a signal other
// threads raise through WakeUp().
class WXDLLIMPEXP_CORE wxGUIEventLoop : public wxEventLoopManual
{
public:
    wxGUIEventLoop();

    virtual bool Pending() const wxOVERRIDE;
    virtual bool Dispatch() wxOVERRIDE;
    virtual int DispatchTimeout(unsigned long timeout) wxOVERRIDE;
    virtual void WakeUp() wxOVERRIDE;

protected:
    virtual void DoYieldFor(long eventsToProcess) wxOVERRIDE;

private:
    // waits at most timeout ms (-1: until something comes) and handles what
    // came; false when it timed out with nothing
    bool DoDispatch(long timeout);

    wxDECLARE_NO_COPY_CLASS(wxGUIEventLoop);
};

#endif // _WX_OPENLAYOUT_EVTLOOP_H_
