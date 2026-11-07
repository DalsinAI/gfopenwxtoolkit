/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/textctrl.cpp
// Purpose:     wxTextEntry, wxTextCtrl and wxComboBox: olui's fields and
//              textareas (positions are wx's characters, olui's are bytes
//              of UTF-8)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/textctrl.h"
    #include "wx/combobox.h"
    #include "wx/toplevel.h"
    #include "wx/button.h"
#endif

#include "wx/clipbrd.h"
#include "wx/tooltip.h"

#include "private.h"

// a character position in UTF-8 text: its byte, and back
static size_t ByteOf(const char *s, long pos)
{
    size_t b = 0;
    if ( !s )
        return 0;
    for ( long n = 0; s[b] && n < pos; n++ )
    {
        b++;
        while ( (s[b] & 0xc0) == 0x80 )
            b++;
    }
    return b;
}

static long CharOf(const char *s, size_t byte)
{
    long n = 0;
    if ( !s )
        return 0;
    for ( size_t b = 0; b < byte && s[b]; b++ )
        if ( (s[b] & 0xc0) != 0x80 )
            n++;
    return n;
}

// ---- wxTextEntry -------------------------------------------------------------------------

struct olui_el *wxTextEntry::OLGetTextElement() const
{
    const wxWindow *w = dynamic_cast<const wxWindow *>(this);
    return w ? w->OLGetElement() : NULL;
}

static struct olui *UIOf(const wxTextEntry *e)
{
    const wxWindow *w = dynamic_cast<const wxWindow *>(e);
    return w ? w->OLGetUI() : NULL;
}

wxWindow *wxTextEntry::GetEditableWindow()
{
    return dynamic_cast<wxWindow *>(this);
}

wxString wxTextEntry::DoGetValue() const
{
    struct olui_el *el = OLGetTextElement();
    if ( el )
    {
        const char *t = olui_get_text(el);
        return wxOLFromUTF8(t ? t : "");
    }
    return m_olValue;
}

void wxTextEntry::DoSetValue(const wxString& value, int flags)
{
    m_olValue = value;
    struct olui_el *el = OLGetTextElement();
    struct olui *ui = UIOf(this);
    if ( el && ui )
    {
        olui_set_text(ui, el, wxOLToUTF8(value).c_str());
        const size_t end = strlen(olui_get_text(el) ? olui_get_text(el) : "");
        olui_set_selection(ui, el, end, end);
    }
    if ( flags & SetValue_SendEvent )
        SendTextUpdatedEventIfAllowed();
}

void wxTextEntry::WriteText(const wxString& text)
{
    long from, to;
    GetSelection(&from, &to);
    wxString v = DoGetValue();
    v = v.Left(from) + text + v.Mid(to);
    DoSetValue(v, SetValue_SendEvent);
    SetInsertionPoint(from + (long)text.length());
}

void wxTextEntry::Remove(long from, long to)
{
    wxString v = DoGetValue();
    if ( to < 0 || to > (long)v.length() )
        to = (long)v.length();
    if ( from < 0 || from >= to )
        return;
    DoSetValue(v.Left(from) + v.Mid(to), SetValue_SendEvent);
    SetInsertionPoint(from);
}

void wxTextEntry::Copy()
{
#if wxUSE_CLIPBOARD
    wxString s = GetStringSelection();
    if ( !s.empty() && wxTheClipboard->Open() )
    {
        wxTheClipboard->SetData(new wxTextDataObject(s));
        wxTheClipboard->Close();
    }
#endif
}

void wxTextEntry::Cut()
{
    if ( !IsEditable() )
        return;
    long from, to;
    GetSelection(&from, &to);
    if ( from == to )
        return;
    Copy();
    Remove(from, to);
}

void wxTextEntry::Paste()
{
#if wxUSE_CLIPBOARD
    if ( !IsEditable() || !wxTheClipboard->Open() )
        return;
    wxTextDataObject data;
    if ( wxTheClipboard->IsSupported(wxDF_UNICODETEXT) || wxTheClipboard->IsSupported(wxDF_TEXT) )
        wxTheClipboard->GetData(data);
    wxTheClipboard->Close();
    if ( !data.GetText().empty() )
        WriteText(data.GetText());
#endif
}

void wxTextEntry::SetInsertionPoint(long pos)
{
    SetSelection(pos, pos);
}

long wxTextEntry::GetInsertionPoint() const
{
    struct olui_el *el = OLGetTextElement();
    if ( !el )
        return (long)m_olValue.length();
    size_t from, to;
    olui_get_selection(el, &from, &to);
    return CharOf(olui_get_text(el), to);
}

long wxTextEntry::GetLastPosition() const
{
    return (long)DoGetValue().length();
}

void wxTextEntry::SetSelection(long from, long to)
{
    struct olui_el *el = OLGetTextElement();
    struct olui *ui = UIOf(this);
    if ( !el || !ui )
        return;
    const char *t = olui_get_text(el);
    if ( from == -1 && to == -1 )
    {
        from = 0;
        to = GetLastPosition();
    }
    if ( to == -1 )
        to = GetLastPosition();
    olui_set_selection(ui, el, ByteOf(t, from), ByteOf(t, to));
}

void wxTextEntry::GetSelection(long *from, long *to) const
{
    struct olui_el *el = OLGetTextElement();
    if ( !el )
    {
        if ( from ) *from = (long)m_olValue.length();
        if ( to ) *to = (long)m_olValue.length();
        return;
    }
    size_t a, b;
    olui_get_selection(el, &a, &b);
    if ( a > b )
    {
        size_t c = a;
        a = b;
        b = c;
    }
    const char *t = olui_get_text(el);
    if ( from ) *from = CharOf(t, a);
    if ( to ) *to = CharOf(t, b);
}

void wxTextEntry::SetEditable(bool editable)
{
    if ( editable == m_olEditable )
        return;
    m_olValue = DoGetValue();
    m_olEditable = editable;
    // readonly is in the markup
    if ( wxWindow *w = dynamic_cast<wxWindow *>(this) )
        w->OLRecreate();
}

// what a field asks for: the clipboard (olui's Amiga-C, X and V)
static void ClipboardEvent(wxWindow *win, const struct olui_event& ev)
{
#if wxUSE_CLIPBOARD
    if ( ev.type == OLUI_CUT || ev.type == OLUI_COPY )
    {
        if ( ev.value && wxTheClipboard->Open() )
        {
            wxTheClipboard->SetData(new wxTextDataObject(wxOLFromUTF8(ev.value)));
            wxTheClipboard->Close();
        }
        wxClipboardTextEvent ce(ev.type == OLUI_CUT ? wxEVT_TEXT_CUT : wxEVT_TEXT_COPY, win->GetId());
        ce.SetEventObject(win);
        win->HandleWindowEvent(ce);
    }
    else if ( ev.type == OLUI_PASTE )
    {
        wxClipboardTextEvent ce(wxEVT_TEXT_PASTE, win->GetId());
        ce.SetEventObject(win);
        if ( win->HandleWindowEvent(ce) )
            return;
        wxTextDataObject data;
        if ( wxTheClipboard->Open() )
        {
            if ( wxTheClipboard->IsSupported(wxDF_UNICODETEXT) || wxTheClipboard->IsSupported(wxDF_TEXT) )
                wxTheClipboard->GetData(data);
            wxTheClipboard->Close();
        }
        if ( !data.GetText().empty() && ev.el && win->OLGetUI() )
            olui_insert_text(win->OLGetUI(), ev.el, wxOLToUTF8(data.GetText()).c_str());
    }
#else
    (void)win;
    (void)ev;
#endif
}

// Return in a field that doesn't take it: the dialog's default button
static void PressDefault(wxWindow *win)
{
    wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(win), wxTopLevelWindow);
    wxWindow *def = tlw ? tlw->GetDefaultItem() : NULL;
    wxButton *b = def ? wxDynamicCast(def, wxButton) : NULL;
    if ( b && b->IsEnabled() )
    {
        wxCommandEvent e(wxEVT_BUTTON, b->GetId());
        e.SetEventObject(b);
        b->HandleWindowEvent(e);
    }
}

// ---- wxTextCtrl -------------------------------------------------------------------------------

// (wxTextCtrl's class info is in wx's common code)

bool wxTextCtrl::Create(wxWindow *parent, wxWindowID id, const wxString& value, const wxPoint& pos,
                        const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_olValue = value;
    m_olEditable = !(style & wxTE_READONLY);
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxTextCtrl::OLGetMarkup() const
{
    wxString a = " id=" + OLId();
    if ( GetToolTip() && !GetToolTip()->GetTip().empty() )
        a += " title=\"" + wxOLEscape(GetToolTip()->GetTip()) + "\"";
    if ( !m_olEditable )
        a += " readonly";
    if ( !IsThisEnabled() )
        a += " disabled";
    if ( m_olMaxLength > 0 )
        a += wxString::Format(" maxlength=%lu", m_olMaxLength);
    if ( !GetHint().empty() )
        a += " placeholder=\"" + wxOLEscape(GetHint()) + "\"";
    if ( IsMultiLine() )
    {
        if ( HasFlag(wxHSCROLL) || HasFlag(wxTE_DONTWRAP) )
            a += " wrap=off";
        return "<textarea" + a + ">" + wxOLEscape(m_olValue) + "</textarea>";
    }
    if ( HasFlag(wxTE_PASSWORD) )
        a += " type=password";
    wxString align;
    if ( HasFlag(wxTE_RIGHT) )
        align = " style='text-align:right'";
    else if ( HasFlag(wxTE_CENTRE) )
        align = " style='text-align:center'";
    return "<input" + a + align + " value=\"" + wxOLEscape(m_olValue) + "\">";
}

void wxTextCtrl::OLHandleEvent(const struct olui_event& ev)
{
    switch ( ev.type )
    {
        case OLUI_CHANGE:
        {
            const char *t = ev.value ? ev.value : (m_olEl ? olui_get_text(m_olEl) : NULL);
            m_olValue = wxOLFromUTF8(t ? t : "");
            m_olModified = true;
            SendTextUpdatedEventIfAllowed();
            break;
        }
        case OLUI_SUBMIT:
            if ( HasFlag(wxTE_PROCESS_ENTER) )
            {
                wxCommandEvent e(wxEVT_TEXT_ENTER, GetId());
                e.SetString(GetValue());
                e.SetEventObject(this);
                if ( HandleWindowEvent(e) )
                    break;
            }
            PressDefault(this);
            break;
        case OLUI_CUT:
        case OLUI_COPY:
        case OLUI_PASTE:
            ClipboardEvent(this, ev);
            break;
        default:
            break;
    }
}

void wxTextCtrl::DoSetValue(const wxString& value, int flags)
{
    if ( value == DoGetValue() && !(flags & SetValue_SendEvent) )
        return;
    m_olModified = false;
    if ( m_olEl && IsMultiLine() )
    {
        // a textarea's text with newlines: olui_set_text keeps them
        wxTextEntry::DoSetValue(value, flags);
        return;
    }
    wxTextEntry::DoSetValue(value, flags);
}

wxSize wxTextCtrl::DoGetBestSize() const
{
    int w, h;
    GetTextExtent("W", &w, &h);
    if ( IsMultiLine() )
        return wxSize(w * 20 + 12, h * 5 + 12);
    return wxSize(w * 12 + 12, h + 10);
}

int wxTextCtrl::GetLineLength(long lineNo) const
{
    wxArrayString lines = wxSplit(GetValue(), '\n', '\0');
    return lineNo >= 0 && (size_t)lineNo < lines.size() ? (int)lines[lineNo].length() : -1;
}

wxString wxTextCtrl::GetLineText(long lineNo) const
{
    wxArrayString lines = wxSplit(GetValue(), '\n', '\0');
    return lineNo >= 0 && (size_t)lineNo < lines.size() ? lines[lineNo] : wxString();
}

int wxTextCtrl::GetNumberOfLines() const
{
    return (int)wxSplit(GetValue(), '\n', '\0').size();
}

bool wxTextCtrl::SetStyle(long WXUNUSED(start), long WXUNUSED(end), const wxTextAttr& WXUNUSED(style))
{
    // olui's text is in one style
    return false;
}

bool wxTextCtrl::GetStyle(long WXUNUSED(position), wxTextAttr& style)
{
    style = m_defaultStyle;
    return false;
}

bool wxTextCtrl::SetDefaultStyle(const wxTextAttr& style)
{
    return wxTextCtrlBase::SetDefaultStyle(style);
}

long wxTextCtrl::XYToPosition(long x, long y) const
{
    wxArrayString lines = wxSplit(GetValue(), '\n', '\0');
    if ( y < 0 || (size_t)y >= lines.size() || x < 0 || x > (long)lines[y].length() )
        return -1;
    long pos = 0;
    for ( long i = 0; i < y; i++ )
        pos += (long)lines[i].length() + 1;
    return pos + x;
}

bool wxTextCtrl::PositionToXY(long pos, long *x, long *y) const
{
    const wxString v = GetValue();
    if ( pos < 0 || pos > (long)v.length() )
        return false;
    long line = 0, col = 0;
    for ( long i = 0; i < pos; i++ )
    {
        if ( v[i] == '\n' )
        {
            line++;
            col = 0;
        }
        else
            col++;
    }
    if ( x ) *x = col;
    if ( y ) *y = line;
    return true;
}

void wxTextCtrl::ShowPosition(long pos)
{
    // a caret set is scrolled into view
    long from, to;
    GetSelection(&from, &to);
    SetSelection(pos, pos);
    SetSelection(from, to);
}

// ---- wxComboBox ---------------------------------------------------------------------------------

// (wxComboBox's class info is in wx's common code)

bool wxComboBox::Create(wxWindow *parent, wxWindowID id, const wxString& value, const wxPoint& pos,
                        const wxSize& size, int n, const wxString choices[], long style,
                        const wxValidator& validator, const wxString& name)
{
    m_olValue = value;
    m_olEditable = !(style & wxCB_READONLY);
    for ( int i = 0; i < n; i++ )
    {
        m_olItems.push_back(choices[i]);
        m_olData.push_back(NULL);
    }
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    if ( !value.empty() )
    {
        const int i = FindString(value);
        if ( i != wxNOT_FOUND )
            wxChoice::SetSelection(i);
    }
    SetInitialSize(size);
    return true;
}

bool wxComboBox::Create(wxWindow *parent, wxWindowID id, const wxString& value, const wxPoint& pos,
                        const wxSize& size, const wxArrayString& choices, long style,
                        const wxValidator& validator, const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, value, pos, size, chs.GetCount(), chs.GetStrings(), style, validator, name);
}

wxString wxComboBox::OLGetMarkup() const
{
    // a field and a narrow drop-down beside it; read-only, just the drop-down
    wxString opts;
    for ( size_t i = 0; i < m_olItems.size(); i++ )
        opts += wxString::Format("<option%s>", (int)i == m_olSel ? " selected" : "") + wxOLEscape(m_olItems[i]) +
                "</option>";
    const wxString dis = IsThisEnabled() ? "" : " disabled";
    if ( HasFlag(wxCB_READONLY) )
        return "<select id=" + OLId() + dis + ">" + opts + "</select>";
    // the field gives way to the drop-down: olui's input is 160 pixels wide
    // and does not shrink below that with flex:1 alone, so a narrower combo
    // box pushed its drop-down out of the box
    return "<div id=" + OLId() + " style='display:flex;gap:2px'><input id=" + OLId() + "t" + dis +
           " style='flex:1 1 0;width:auto;min-width:0' value=\"" + wxOLEscape(m_olValue) + "\"><select id=" + OLId() + "s" + dis +
           " style='width:20px'>" + opts + "</select></div>";
}

struct olui_el *wxComboBox::OLGetTextElement() const
{
    if ( HasFlag(wxCB_READONLY) || !m_olEl || !OLGetUI() )
        return NULL;
    return olui_find(OLGetUI(), (OLId() + "t").utf8_str());
}

void wxComboBox::OLHandleEvent(const struct olui_event& ev)
{
    const wxString id = wxOLFromUTF8(ev.id);
    if ( HasFlag(wxCB_READONLY) || id == OLId() + "s" )
    {
        if ( ev.type != OLUI_CHANGE || ev.index < 0 || (size_t)ev.index >= m_olItems.size() )
            return;
        m_olSel = ev.index;
        if ( !HasFlag(wxCB_READONLY) )
            wxTextEntry::DoSetValue(m_olItems[m_olSel], 0);
        m_olValue = m_olItems[m_olSel];
        wxCommandEvent e(wxEVT_COMBOBOX, GetId());
        e.SetInt(m_olSel);
        e.SetString(m_olValue);
        e.SetEventObject(this);
        HandleWindowEvent(e);
        SendTextUpdatedEventIfAllowed();
        return;
    }
    switch ( ev.type )
    {
        case OLUI_CHANGE:
            m_olValue = wxOLFromUTF8(ev.value ? ev.value : "");
            m_olSel = FindString(m_olValue);
            SendTextUpdatedEventIfAllowed();
            break;
        case OLUI_SUBMIT:
            if ( HasFlag(wxTE_PROCESS_ENTER) )
            {
                wxCommandEvent e(wxEVT_TEXT_ENTER, GetId());
                e.SetString(GetValue());
                e.SetEventObject(this);
                if ( HandleWindowEvent(e) )
                    break;
            }
            PressDefault(this);
            break;
        case OLUI_CUT:
        case OLUI_COPY:
        case OLUI_PASTE:
            ClipboardEvent(this, ev);
            break;
        default:
            break;
    }
}

void wxComboBox::SetSelection(int n)
{
    wxChoice::SetSelection(n);
    if ( n >= 0 && (size_t)n < m_olItems.size() )
        wxTextEntry::DoSetValue(m_olItems[n], 0);
}

void wxComboBox::Clear()
{
    wxTextEntry::Clear();
    wxItemContainer::Clear();
}

void wxComboBox::Popup()
{
    struct olui *ui = OLGetUI();
    struct olui_el *sel = !m_olEl || !ui ? NULL
                          : HasFlag(wxCB_READONLY) ? m_olEl : olui_find(ui, (OLId() + "s").utf8_str());
    if ( sel )
        olui_select_open(ui, sel, 1);
}

void wxComboBox::Dismiss()
{
    struct olui *ui = OLGetUI();
    struct olui_el *sel = !m_olEl || !ui ? NULL
                          : HasFlag(wxCB_READONLY) ? m_olEl : olui_find(ui, (OLId() + "s").utf8_str());
    if ( sel )
        olui_select_open(ui, sel, 0);
}

wxSize wxComboBox::DoGetBestSize() const
{
    wxSize s = wxChoice::DoGetBestSize();
    if ( !HasFlag(wxCB_READONLY) )
        s.x += 24;
    return s;
}
