/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/arexx.h
// Purpose:     wxARexxPort: a program's ARexx port, on arexx.class
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_AREXX_H_
#define _WX_OPENLAYOUT_AREXX_H_

// One ARexx port for a wx program, the way every one of our programs has
// it: arexx.class (OS 3.2's or ours) holds the port, reads each command's
// arguments with its ReadArgs template and sends the replies. The program
// adds its own commands; the port has these from the start:
//
//   QUIT FORCE/S       closes the main window (FORCE: it cannot refuse)
//   ACTIVATE           the main window to the front, out of its icon
//   DEACTIVATE         the main window to its icon
//   VERSION            RESULT: the program's name and version
//   HELP COMMAND       RESULT: the commands, or one command's template
//   FAULT CODE/N/A     RESULT: the text of a DOS error number
//
// A command of the program's own with one of those names is used instead.
// The port is named after the program in capitals with the first free
// slot: FILEZILLA.1, then FILEZILLA.2 for a second copy. Commands run on
// the main thread, from the event loop, like any wx event. RC is 0, or
// what SetError gives (5 a warning, 10 an error, 20 a failure); RESULT goes
// back only when the script asked for it (OPTIONS RESULTS).
//
//   m_rexx = new wxARexxPort("FILEZILLA", "FileZilla 3.69.1");
//   m_rexx->Add("CONNECT", "SITE/A", [this](wxARexxCommand& c) {
//       if ( !Connect(c.Str(0)) )
//           c.SetError(10);
//   });
//   m_rexx->Open();

#include "wx/string.h"
#include "wx/arrstr.h"

#include <functional>
#include <string>
#include <vector>

class WXDLLIMPEXP_CORE wxARexxCommand
{
public:
    // the command's name, as the table has it
    wxString GetName() const { return m_name; }

    // argument i of the template (0 the first): whether it was given
    bool Has(int i) const;
    // a string argument (/A, /K or plain), "" when not given
    wxString Str(int i) const;
    // a switch (/S)
    bool Switch(int i) const;
    // a number (/N), def when not given
    long Number(int i, long def = 0) const;
    // a list (/M)
    wxArrayString Multi(int i) const;

    // RESULT, when the script asked for one
    void SetResult(const wxString& result);
    // RC (and RC2), instead of 0
    void SetError(long rc, long rc2 = 0);

    // for the port
    wxARexxCommand(const wxString& name, void *args) : m_name(name), m_args(args), m_rc(0), m_rc2(0) { }
    wxString m_name;
    void *m_args;
    long m_rc, m_rc2;
    wxString m_result;
    bool m_hasResult = false;
};

typedef std::function<void(wxARexxCommand&)> wxARexxHandler;

class WXDLLIMPEXP_CORE wxARexxPort
{
public:
    // name: the port's name in capitals, without the slot; version: what
    // VERSION answers ("" for the application's display name)
    wxARexxPort(const wxString& name, const wxString& version = wxString());
    ~wxARexxPort();

    // a command: its name, its ReadArgs template ("" for none) and what
    // runs it. Before Open().
    void Add(const wxString& name, const wxString& argTemplate, wxARexxHandler handler);

    // the port opens: false when arexx.class or the port can't be had (the
    // program goes on without one)
    bool Open();
    bool IsOk() const { return m_object != NULL; }

    // the port's full name, with its slot (FILEZILLA.1), once open
    wxString GetPortName() const { return m_portName; }

    // for the event loop
    void HandleEvent();
    void RunCommand(unsigned id, void *cmd);

private:
    struct Entry { std::string name, tmpl; wxARexxHandler handler; };

    void AddStandard();
    void Close();

    wxString m_name, m_version, m_portName;
    std::vector<Entry> m_entries;
    void *m_table;          // struct ARexxCmd[], ended by an empty one
    void *m_thunk;          // ac_Func: A4 set, then RunCommand
    void *m_object;
    unsigned long m_sigMask;
    std::string m_result;   // ac_Result, until the next command
    bool m_busy, m_again;

    wxARexxPort(const wxARexxPort&) = delete;
    wxARexxPort& operator=(const wxARexxPort&) = delete;
};

#endif // _WX_OPENLAYOUT_AREXX_H_
