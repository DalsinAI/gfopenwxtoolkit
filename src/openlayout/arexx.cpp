/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/arexx.cpp
// Purpose:     wxARexxPort: a program's ARexx port, on arexx.class
//              (wx/openlayout/arexx.h)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/toplevel.h"
    #include "wx/dialog.h"
#endif

#include "wx/evtloop.h"
#include "wx/time.h"
#include "wx/timer.h"

#include "wx/openlayout/arexx.h"

#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <classes/arexx.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>

#include "private.h"

#include <string.h>

void wxOLAddSignalWatch(unsigned long mask, void (*fn)(void *), void *data);
void wxOLRemoveSignalWatch(void *data);

// ---- arexx.class ---------------------------------------------------------------------

// arexx.class, through our own base: AREXX_GetClass at -30
static struct Library *gs_arexxBase = NULL;
static int gs_arexxUsers = 0;
#define wxOL_AREXX_GetClass() LP0(0x1e, Class *, AREXX_GetClass, , gs_arexxBase)

static Class *OpenARexxClass()
{
    if ( !gs_arexxBase )
    {
        gs_arexxBase = OpenLibrary((STRPTR)"arexx.class", 44);
        if ( !gs_arexxBase )
            gs_arexxBase = OpenLibrary((STRPTR)"classes/arexx.class", 44);
        if ( !gs_arexxBase )
            return NULL;
    }
    gs_arexxUsers++;
    return wxOL_AREXX_GetClass();
}

static void CloseARexxClass()
{
    if ( gs_arexxUsers > 0 && --gs_arexxUsers == 0 && gs_arexxBase )
    {
        CloseLibrary(gs_arexxBase);
        gs_arexxBase = NULL;
    }
}

// ---- ac_Func --------------------------------------------------------------------------

// arexx.class calls a command's ac_Func with A0 the command and A1 the
// message, from inside AM_HANDLEEVENT. With wx in openwxtoolkit.library's
// shared module, wx's code reaches its data through A4, which the class
// need not keep: so ac_Func is a piece of code made for this port that sets
// A4 to the program's copy (as wxtk_module_rt.c's thunks do), pushes the
// two as C arguments, and calls Dispatch(port-cmd, message).
extern "C" void wxOLARexxDispatch(struct ARexxCmd *cmd, struct RexxMsg *msg);

static const UWORD gs_thunkCode[] = {
    0x2F0C,                 // move.l a4,-(sp)
    0x2F09,                 // move.l a1,-(sp)
    0x2F08,                 // move.l a0,-(sp)
    0x287C, 0, 0,           // movea.l #a4,a4
    0x4EB9, 0, 0,           // jsr wxOLARexxDispatch
    0x508F,                 // addq.l #8,sp
    0x285F,                 // move.l (sp)+,a4
    0x4E75,                 // rts
};

static APTR CurrentA4()
{
    APTR a4;
    __asm__ __volatile__("move.l %%a4,%0" : "=d"(a4));
    return a4;
}

static void *MakeThunk()
{
    UWORD *t = (UWORD *)AllocVec(sizeof gs_thunkCode, MEMF_ANY);
    if ( !t )
        return NULL;
    CopyMem((APTR)gs_thunkCode, t, sizeof gs_thunkCode);
    *(APTR *)&t[4] = CurrentA4();
    *(APTR *)&t[7] = (APTR)&wxOLARexxDispatch;
    CacheClearU();
    return t;
}

// the port a command belongs to: ARexxCmd has no room for it, so the ports
// (few: one a program, as a rule) and their tables are kept side by side
static std::vector<wxARexxPort *> gs_ports;
static std::vector<struct ARexxCmd *> gs_tables;

extern "C" void wxOLARexxDispatch(struct ARexxCmd *cmd, struct RexxMsg *WXUNUSED(msg))
{
    for ( size_t i = 0; i < gs_tables.size(); i++ )
    {
        struct ARexxCmd *t = gs_tables[i];
        if ( cmd == t + cmd->ac_ID )
        {
            gs_ports[i]->RunCommand(cmd->ac_ID, cmd);
            return;
        }
    }
    cmd->ac_RC = RC_FATAL;
}

// ---- wxARexxCommand ------------------------------------------------------------------

static inline LONG ArgAt(void *args, int i)
{
    return args && i >= 0 && i < 32 ? ((LONG *)args)[i] : 0;
}

bool wxARexxCommand::Has(int i) const
{
    return ArgAt(m_args, i) != 0;
}

wxString wxARexxCommand::Str(int i) const
{
    const char *s = (const char *)ArgAt(m_args, i);
    return s ? wxString(s, wxConvISO8859_1) : wxString();
}

bool wxARexxCommand::Switch(int i) const
{
    return ArgAt(m_args, i) != 0;
}

long wxARexxCommand::Number(int i, long def) const
{
    const LONG *n = (const LONG *)ArgAt(m_args, i);
    return n ? *n : def;
}

wxArrayString wxARexxCommand::Multi(int i) const
{
    wxArrayString a;
    const char **m = (const char **)ArgAt(m_args, i);
    for ( ; m && *m; m++ )
        a.Add(wxString(*m, wxConvISO8859_1));
    return a;
}

void wxARexxCommand::SetResult(const wxString& result)
{
    m_result = result;
    m_hasResult = true;
}

void wxARexxCommand::SetError(long rc, long rc2)
{
    m_rc = rc;
    m_rc2 = rc2;
}

// ---- wxARexxPort ---------------------------------------------------------------------

static void PortSignal(void *data)
{
    static_cast<wxARexxPort *>(data)->HandleEvent();
}

wxARexxPort::wxARexxPort(const wxString& name, const wxString& version)
    : m_name(name.Upper()), m_version(version), m_table(NULL), m_thunk(NULL),
      m_object(NULL), m_sigMask(0), m_busy(false), m_again(false)
{
}

wxARexxPort::~wxARexxPort()
{
    Close();
}

void wxARexxPort::Add(const wxString& name, const wxString& argTemplate, wxARexxHandler handler)
{
    if ( m_object )
        return;
    const std::string n = wxOLToLatin1(name.Upper());
    for ( Entry& e : m_entries )
    {
        if ( e.name == n )
        {
            e.tmpl = wxOLToLatin1(argTemplate);
            e.handler = handler;
            return;
        }
    }
    m_entries.push_back(Entry{n, wxOLToLatin1(argTemplate), handler});
}

static wxTopLevelWindow *MainWindow()
{
    return wxTheApp ? wxDynamicCast(wxTheApp->GetTopWindow(), wxTopLevelWindow) : NULL;
}

// QUIT while a dialog's own loop runs (a file requester is up): closing the
// main window there deletes the dialog under its ShowModal, and the memory
// list is found broken at exit (Guru 81000005). The dialogs end as Cancel,
// and a timer closes the window once their loops are over (a CallAfter
// could run again inside the ending loop and queue itself for ever).
static bool InModalLoop()
{
    wxEventLoopBase *loop = wxEventLoopBase::GetActive();
    return wxTheApp && loop && loop != wxTheApp->GetMainLoop();
}

static void Quit(bool force, wxARexxCommand *c)
{
    if ( InModalLoop() )
    {
        for ( wxWindowList::compatibility_iterator n = wxTopLevelWindows.GetFirst(); n; n = n->GetNext() )
        {
            wxDialog *d = wxDynamicCast(n->GetData(), wxDialog);
            if ( d && d->IsModal() )
                d->EndModal(wxID_CANCEL);
        }
        wxTimer *t = new wxTimer();
        t->Bind(wxEVT_TIMER, [force, t](wxTimerEvent&) {
            if ( InModalLoop() )
                return;
            t->Stop();
            wxTheApp->CallAfter([t] { delete t; });
            Quit(force, NULL);
        });
        t->Start(100);
        return;
    }
    wxTopLevelWindow *w = MainWindow();
    if ( !w )
    {
        if ( wxTheApp )
            wxTheApp->ExitMainLoop();
        return;
    }
    if ( !w->Close(force) && !force )
    {
        if ( c )
            c->SetError(RC_WARN);   // the program said no (unsaved work)
    }
    else if ( force && wxTheApp )
        wxTheApp->ExitMainLoop();
}

// the standard commands, where the program has none of the same name
void wxARexxPort::AddStandard()
{
    auto have = [this](const char *n) {
        for ( const Entry& e : m_entries )
            if ( e.name == n )
                return true;
        return false;
    };
    if ( !have("QUIT") )
        m_entries.push_back(Entry{"QUIT", "FORCE/S", [](wxARexxCommand& c) {
            Quit(c.Switch(0), &c);
        }});
    if ( !have("ACTIVATE") )
        m_entries.push_back(Entry{"ACTIVATE", "", [](wxARexxCommand& c) {
            wxTopLevelWindow *w = MainWindow();
            if ( !w )
            {
                c.SetError(RC_WARN);
                return;
            }
            if ( w->IsIconized() )
                w->Iconize(false);
            w->Show();
            w->Raise();
        }});
    if ( !have("DEACTIVATE") )
        m_entries.push_back(Entry{"DEACTIVATE", "", [](wxARexxCommand& c) {
            wxTopLevelWindow *w = MainWindow();
            if ( !w )
            {
                c.SetError(RC_WARN);
                return;
            }
            w->Iconize(true);
        }});
    if ( !have("VERSION") )
        m_entries.push_back(Entry{"VERSION", "", [this](wxARexxCommand& c) {
            wxString v = m_version;
            if ( v.empty() && wxTheApp )
                v = wxTheApp->GetAppDisplayName();
            c.SetResult(v);
        }});
    if ( !have("HELP") )
        m_entries.push_back(Entry{"HELP", "COMMAND", [this](wxARexxCommand& c) {
            const std::string want = wxOLToLatin1(c.Str(0).Upper());
            wxString out;
            for ( const Entry& e : m_entries )
            {
                if ( want.empty() )
                {
                    if ( !out.empty() )
                        out += ' ';
                    out += wxString(e.name.c_str(), wxConvISO8859_1);
                }
                else if ( e.name == want )
                {
                    out = wxString(e.name.c_str(), wxConvISO8859_1);
                    if ( !e.tmpl.empty() )
                        out += ' ' + wxString(e.tmpl.c_str(), wxConvISO8859_1);
                    c.SetResult(out);
                    return;
                }
            }
            if ( !want.empty() )
            {
                c.SetError(RC_ERROR);
                return;
            }
            c.SetResult(out);
        }});
    if ( !have("FAULT") )
        m_entries.push_back(Entry{"FAULT", "CODE/N/A", [](wxARexxCommand& c) {
            char buf[100];
            buf[0] = 0;
            if ( Fault(c.Number(0), NULL, (STRPTR)buf, sizeof buf) <= 0 )
            {
                c.SetError(RC_WARN);
                return;
            }
            c.SetResult(wxString(buf, wxConvISO8859_1));
        }});
}

bool wxARexxPort::Open()
{
    if ( m_object )
        return true;
    AddStandard();

    Class *cl = OpenARexxClass();
    if ( !cl )
        return false;

    const size_t n = m_entries.size();
    struct ARexxCmd *t = (struct ARexxCmd *)AllocVec((n + 1) * sizeof(struct ARexxCmd), MEMF_ANY | MEMF_CLEAR);
    m_thunk = MakeThunk();
    if ( !t || !m_thunk )
    {
        if ( t )
            FreeVec(t);
        Close();
        CloseARexxClass();
        return false;
    }
    for ( size_t i = 0; i < n; i++ )
    {
        t[i].ac_Name = (STRPTR)m_entries[i].name.c_str();
        t[i].ac_ID = (UWORD)i;
        t[i].ac_ArgTemplate = m_entries[i].tmpl.empty() ? NULL : (STRPTR)m_entries[i].tmpl.c_str();
        t[i].ac_Func = (void (*)(struct ARexxCmd *, struct RexxMsg *))m_thunk;
    }
    m_table = t;
    gs_ports.push_back(this);
    gs_tables.push_back(t);

    const std::string name = wxOLToLatin1(m_name);
    struct TagItem tags[] = {
        { AREXX_HostName, (ULONG)name.c_str() },
        { AREXX_Commands, (ULONG)t },
        { TAG_DONE, 0 },
    };
    m_object = NewObjectA(cl, NULL, tags);
    if ( !m_object )
    {
        Close();
        CloseARexxClass();
        return false;
    }

    ULONG sig = 0;
    STRPTR full = NULL;
    GetAttr(AREXX_SigMask, (Object *)m_object, &sig);
    GetAttr(AREXX_HostName, (Object *)m_object, (ULONG *)&full);
    m_sigMask = sig;
    m_portName = full ? wxString((const char *)full, wxConvISO8859_1) : m_name;
    wxOLAddSignalWatch(m_sigMask, PortSignal, this);
    return true;
}

void wxARexxPort::Close()
{
    if ( m_object )
    {
        wxOLRemoveSignalWatch(this);
        DisposeObject((Object *)m_object);
        m_object = NULL;
        CloseARexxClass();
    }
    for ( size_t i = 0; i < gs_ports.size(); i++ )
    {
        if ( gs_ports[i] == this )
        {
            gs_ports.erase(gs_ports.begin() + i);
            gs_tables.erase(gs_tables.begin() + i);
            break;
        }
    }
    if ( m_table )
    {
        FreeVec(m_table);
        m_table = NULL;
    }
    if ( m_thunk )
    {
        FreeVec(m_thunk);
        m_thunk = NULL;
    }
}

// the port's signal came: the class runs what has arrived. A command that
// opens a window of its own (QUIT asking about unsaved work) runs a loop of
// its own, and that loop must not run the class again inside the first
// command: what comes meanwhile waits, and is run after.
void wxARexxPort::HandleEvent()
{
    if ( !m_object )
        return;
    if ( m_busy )
    {
        m_again = true;
        return;
    }
    m_busy = true;
    do
    {
        m_again = false;
        struct { ULONG MethodID; } msg = { AM_HANDLEEVENT };
        DoMethodA((Object *)m_object, (Msg)&msg);
    } while ( m_again && m_object );
    m_busy = false;
}

bool wxARexxPort::WaitFor(wxARexxCommand& cmd, const std::function<bool()>& ready, long timeoutMs)
{
    const wxLongLong until = wxGetLocalTimeMillis() + timeoutMs;
    for ( ;; )
    {
        if ( ready() )
            return true;
        wxTopLevelWindow *w = MainWindow();
        wxEventLoopBase *loop = wxEventLoopBase::GetActive();
        if ( !wxTheApp || !loop || (w && w->IsBeingDeleted()) )
        {
            cmd.SetError(RC_FATAL);
            return false;
        }
        const long left = (until - wxGetLocalTimeMillis()).ToLong();
        if ( left <= 0 )
        {
            cmd.SetError(RC_ERROR);
            return false;
        }
        // the loop's own dispatch runs messages, timers and sockets, but not
        // the events queued for later or the idle handlers: those run
        // between the main loop's dispatches, so they are run here too
        // (wxMaxima and Code::Blocks read their tools' output in idle
        // handlers; Team 2, 10 Nov 2026)
        if ( loop->DispatchTimeout(left < 50 ? left : 50) < 0 )
        {
            cmd.SetError(RC_FATAL);     // the loop is ending
            return false;
        }
        wxTheApp->ProcessPendingEvents();
        wxTheApp->ProcessIdle();
    }
}

void wxARexxPort::RunCommand(unsigned id, void *p)
{
    struct ARexxCmd *cmd = (struct ARexxCmd *)p;
    if ( id >= m_entries.size() || !m_entries[id].handler )
    {
        cmd->ac_RC = RC_ERROR;
        return;
    }
    wxARexxCommand c(wxString(m_entries[id].name.c_str(), wxConvISO8859_1), cmd->ac_ArgList);
    m_entries[id].handler(c);
    cmd->ac_RC = c.m_rc;
    cmd->ac_RC2 = c.m_rc2;
    if ( c.m_hasResult && c.m_rc == 0 )
    {
        m_result = wxOLToLatin1(c.m_result);
        cmd->ac_Result = (STRPTR)m_result.c_str();
    }
}
