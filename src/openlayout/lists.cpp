/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/lists.cpp
// Purpose:     wxChoice (an olui select), wxListBox and wxCheckListBox (an
//              olui vlist, whose rows wx gives as they are drawn),
//              wxRadioBox (radio buttons in a fieldset) and wxNotebook
//              (olui tabs over the pages)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/choice.h"
    #include "wx/listbox.h"
    #include "wx/checklst.h"
    #include "wx/radiobox.h"
    #include "wx/combobox.h"
#endif

#include "wx/notebook.h"
#include "wx/nonownedwnd.h"
#include "wx/tooltip.h"

#include "private.h"

static wxString DisabledAttr(const wxWindow *w)
{
    wxString a;
    if ( !w->IsThisEnabled() )
        a += " disabled";
    if ( w->GetToolTip() && !w->GetToolTip()->GetTip().empty() )
        a += " title=\"" + wxOLEscape(w->GetToolTip()->GetTip()) + "\"";
    return a;
}

// the width the longest of the strings takes
static int Widest(const wxWindow *w, const wxArrayString& items)
{
    int widest = 0;
    for ( size_t i = 0; i < items.size(); i++ )
    {
        int x, y;
        w->GetTextExtent(items[i], &x, &y);
        widest = wxMax(widest, x);
    }
    return widest;
}

// ---- wxChoice ---------------------------------------------------------------------------------

// (wxChoice's class info is in wx's common code)

bool wxChoice::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, int n,
                      const wxString choices[], long style, const wxValidator& validator, const wxString& name)
{
    for ( int i = 0; i < n; i++ )
    {
        m_olItems.push_back(choices[i]);
        m_olData.push_back(NULL);
    }
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

bool wxChoice::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                      const wxArrayString& choices, long style, const wxValidator& validator,
                      const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, pos, size, chs.GetCount(), chs.GetStrings(), style, validator, name);
}

wxString wxChoice::OLGetMarkup() const
{
    wxString m = "<select id=" + OLId() + DisabledAttr(this) + ">";
    for ( size_t i = 0; i < m_olItems.size(); i++ )
        m += wxString((int)i == m_olSel ? "<option selected>" : "<option>") + wxOLEscape(m_olItems[i]) + "</option>";
    return m + "</select>";
}

void wxChoice::OLElementMade()
{
    if ( m_olSel != wxNOT_FOUND )
        olui_select_set(OLGetUI(), m_olEl, m_olSel);
}

// the select itself: a combo box's is beside its field
static struct olui_el *SelectOf(const wxChoice *c)
{
    struct olui *ui = c->OLGetUI();
    struct olui_el *el = c->OLGetElement();
    if ( !ui || !el )
        return NULL;
    if ( !strcmp(olui_tag(el), "select") )
        return el;
    return olui_find(ui, (c->OLId() + "s").utf8_str());
}

void wxChoice::OLSyncItems()
{
    struct olui *ui = OLGetUI();
    struct olui_el *sel = SelectOf(this);
    if ( !sel )
        return;
    olui_select_clear(ui, sel);
    for ( size_t i = 0; i < m_olItems.size(); i++ )
        olui_select_add(ui, sel, wxOLToUTF8(m_olItems[i]).c_str(), NULL);
    if ( m_olSel != wxNOT_FOUND )
        olui_select_set(ui, sel, m_olSel);
    InvalidateBestSize();
}

void wxChoice::SetSelection(int n)
{
    m_olSel = n >= 0 && (size_t)n < m_olItems.size() ? n : wxNOT_FOUND;
    if ( struct olui_el *sel = SelectOf(this) )
        olui_select_set(OLGetUI(), sel, m_olSel);
}

void wxChoice::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CHANGE || ev.index < 0 || (size_t)ev.index >= m_olItems.size() )
        return;
    m_olSel = ev.index;
    wxCommandEvent e(wxEVT_CHOICE, GetId());
    InitCommandEventWithItems(e, m_olSel);
    HandleWindowEvent(e);
}

wxSize wxChoice::DoGetBestSize() const
{
    int w, h;
    GetTextExtent("W", &w, &h);
    return wxSize(wxMax(Widest(this, m_olItems), w * 4) + 32, h + 8);
}

wxString wxChoice::GetString(unsigned int n) const
{
    wxCHECK_MSG(n < m_olItems.size(), wxString(), "invalid index");
    return m_olItems[n];
}

void wxChoice::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET(n < m_olItems.size(), "invalid index");
    m_olItems[n] = s;
    OLSyncItems();
}

int wxChoice::DoInsertItems(const wxArrayStringsAdapter& items, unsigned int pos, void **clientData,
                            wxClientDataType type)
{
    for ( unsigned int i = 0; i < items.GetCount(); i++ )
    {
        m_olItems.Insert(items[i], pos + i);
        m_olData.insert(m_olData.begin() + pos + i, (void *)NULL);
        AssignNewItemClientData(pos + i, clientData, i, type);
    }
    if ( m_olSel != wxNOT_FOUND && (unsigned int)m_olSel >= pos )
        m_olSel += items.GetCount();
    OLSyncItems();
    return pos + items.GetCount() - 1;
}

void wxChoice::DoSetItemClientData(unsigned int n, void *clientData)
{
    m_olData[n] = clientData;
}

void *wxChoice::DoGetItemClientData(unsigned int n) const
{
    return m_olData[n];
}

void wxChoice::DoClear()
{
    m_olItems.clear();
    m_olData.clear();
    m_olSel = wxNOT_FOUND;
    OLSyncItems();
}

void wxChoice::DoDeleteOneItem(unsigned int pos)
{
    m_olItems.RemoveAt(pos);
    m_olData.erase(m_olData.begin() + pos);
    if ( m_olSel == (int)pos )
        m_olSel = wxNOT_FOUND;
    else if ( m_olSel > (int)pos )
        m_olSel--;
    OLSyncItems();
}

// ---- wxListBox ---------------------------------------------------------------------------------

// (wxListBox's class info is in wx's common code)

// a row's text, while olui draws it: kept until a few more were asked for
static const char *ListRow(void *userdata, int row, int WXUNUSED(column), int *image)
{
    static std::string ring[8];
    static int next = 0;
    const wxListBox *lb = (const wxListBox *)userdata;
    if ( image )
        *image = 0;
    if ( row < 0 || (unsigned)row >= lb->GetCount() )
        return "";
    std::string& s = ring[next++ & 7];
    s = wxOLToUTF8(lb->OLRowText((unsigned)row));
    return s.c_str();
}

bool wxListBox::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, int n,
                       const wxString choices[], long style, const wxValidator& validator, const wxString& name)
{
    for ( int i = 0; i < n; i++ )
    {
        m_olItems.push_back(choices[i]);
        m_olData.push_back(NULL);
        m_olSelected.push_back(false);
    }
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

bool wxListBox::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                       const wxArrayString& choices, long style, const wxValidator& validator,
                       const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, pos, size, chs.GetCount(), chs.GetStrings(), style, validator, name);
}

wxString wxListBox::OLGetMarkup() const
{
    return "<vlist id=" + OLId() + DisabledAttr(this) + "></vlist>";
}

wxString wxListBox::OLItemMarkup(unsigned int n) const
{
    return m_olItems[n];
}

wxString wxListBox::OLRowText(unsigned int n) const
{
    return OLItemMarkup(n);
}

void wxListBox::OLElementMade()
{
    OLSyncItems();
}

void wxListBox::OLSyncItems()
{
    struct olui *ui = OLGetUI();
    if ( !m_olEl || !ui )
        return;
    olui_vlist_set(ui, m_olEl, (int)m_olItems.size(), ListRow, this);
    for ( size_t i = 0; i < m_olSelected.size(); i++ )
        if ( m_olSelected[i] )
            olui_vlist_select(ui, m_olEl, (int)i, 1);
    olui_repaint(ui, m_olEl, NULL);
    InvalidateBestSize();
}

bool wxListBox::IsSelected(int n) const
{
    return n >= 0 && (size_t)n < m_olSelected.size() && m_olSelected[n];
}

int wxListBox::GetSelections(wxArrayInt& aSelections) const
{
    aSelections.clear();
    for ( size_t i = 0; i < m_olSelected.size(); i++ )
        if ( m_olSelected[i] )
            aSelections.push_back((int)i);
    return (int)aSelections.size();
}

int wxListBox::GetSelection() const
{
    for ( size_t i = 0; i < m_olSelected.size(); i++ )
        if ( m_olSelected[i] )
            return (int)i;
    return wxNOT_FOUND;
}

void wxListBox::DoSetSelection(int n, bool select)
{
    struct olui *ui = OLGetUI();
    const bool single = !HasMultipleSelection();
    if ( n == wxNOT_FOUND || (single && select) )
    {
        for ( size_t i = 0; i < m_olSelected.size(); i++ )
            if ( m_olSelected[i] && (int)i != n )
            {
                m_olSelected[i] = false;
                if ( m_olEl && ui )
                    olui_vlist_select(ui, m_olEl, (int)i, 0);
            }
        if ( n == wxNOT_FOUND )
            return;
    }
    if ( n < 0 || (size_t)n >= m_olSelected.size() )
        return;
    m_olSelected[n] = select;
    if ( m_olEl && ui )
    {
        olui_vlist_select(ui, m_olEl, n, select);
        if ( select )
            olui_vlist_set_current(ui, m_olEl, n);
        olui_repaint(ui, m_olEl, NULL);
    }
}

void wxListBox::DoSetFirstItem(int n)
{
    struct olui *ui = OLGetUI();
    if ( m_olEl && ui )
        olui_set_scroll(ui, m_olEl, n * (GetCharHeight() + 2));
}

void wxListBox::OLHandleEvent(const struct olui_event& ev)
{
    struct olui *ui = OLGetUI();
    if ( !ui || ev.index < 0 || (size_t)ev.index >= m_olItems.size() )
        return;
    if ( ev.type == OLUI_SELECT )
    {
        if ( !HasMultipleSelection() )
        {
            // one at a time: the row clicked
            for ( size_t i = 0; i < m_olSelected.size(); i++ )
                if ( (int)i != ev.index && olui_vlist_selected(m_olEl, (int)i) )
                    olui_vlist_select(ui, m_olEl, (int)i, 0);
        }
        for ( size_t i = 0; i < m_olSelected.size(); i++ )
            m_olSelected[i] = olui_vlist_selected(m_olEl, (int)i) != 0;
        wxCommandEvent e(wxEVT_LISTBOX, GetId());
        InitCommandEventWithItems(e, ev.index);
        e.SetExtraLong(m_olSelected[ev.index]);
        HandleWindowEvent(e);
    }
    else if ( ev.type == OLUI_ACTIVATE )
    {
        wxCommandEvent e(wxEVT_LISTBOX_DCLICK, GetId());
        InitCommandEventWithItems(e, ev.index);
        HandleWindowEvent(e);
    }
}

wxSize wxListBox::DoGetBestSize() const
{
    int w, h;
    GetTextExtent("W", &w, &h);
    const int rows = wxMax(3, wxMin(10, (int)m_olItems.size()));
    return wxSize(wxMax(Widest(this, m_olItems), w * 6) + 28, rows * (h + 2) + 8);
}

wxString wxListBox::GetString(unsigned int n) const
{
    wxCHECK_MSG(n < m_olItems.size(), wxString(), "invalid index");
    return m_olItems[n];
}

void wxListBox::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET(n < m_olItems.size(), "invalid index");
    m_olItems[n] = s;
    if ( m_olEl && OLGetUI() )
        olui_repaint(OLGetUI(), m_olEl, NULL);
}

int wxListBox::DoInsertItems(const wxArrayStringsAdapter& items, unsigned int pos, void **clientData,
                             wxClientDataType type)
{
    for ( unsigned int i = 0; i < items.GetCount(); i++ )
    {
        m_olItems.Insert(items[i], pos + i);
        m_olData.insert(m_olData.begin() + pos + i, (void *)NULL);
        m_olSelected.insert(m_olSelected.begin() + pos + i, false);
        AssignNewItemClientData(pos + i, clientData, i, type);
    }
    OLSyncItems();
    return pos + items.GetCount() - 1;
}

void wxListBox::DoSetItemClientData(unsigned int n, void *clientData)
{
    m_olData[n] = clientData;
}

void *wxListBox::DoGetItemClientData(unsigned int n) const
{
    return m_olData[n];
}

void wxListBox::DoClear()
{
    m_olItems.clear();
    m_olData.clear();
    m_olSelected.clear();
    OLSyncItems();
}

void wxListBox::DoDeleteOneItem(unsigned int pos)
{
    m_olItems.RemoveAt(pos);
    m_olData.erase(m_olData.begin() + pos);
    m_olSelected.erase(m_olSelected.begin() + pos);
    OLSyncItems();
}

// ---- wxCheckListBox ----------------------------------------------------------------------------

// (wxCheckListBox's class info is in wx's common code)

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, int n,
                            const wxString choices[], long style, const wxValidator& validator,
                            const wxString& name)
{
    m_olChecked.assign(n, false);
    return wxListBox::Create(parent, id, pos, size, n, choices, style, validator, name);
}

bool wxCheckListBox::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                            const wxArrayString& choices, long style, const wxValidator& validator,
                            const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, pos, size, chs.GetCount(), chs.GetStrings(), style, validator, name);
}

wxString wxCheckListBox::OLItemMarkup(unsigned int n) const
{
    // the tick in the row's text: [x] or [ ]
    return wxString(n < m_olChecked.size() && m_olChecked[n] ? "[x] " : "[ ] ") + m_olItems[n];
}

bool wxCheckListBox::IsChecked(unsigned int item) const
{
    return item < m_olChecked.size() && m_olChecked[item];
}

void wxCheckListBox::Check(unsigned int item, bool check)
{
    wxCHECK_RET(item < m_olChecked.size(), "invalid index");
    m_olChecked[item] = check;
    if ( m_olEl && OLGetUI() )
        olui_repaint(OLGetUI(), m_olEl, NULL);
}

void wxCheckListBox::OLHandleEvent(const struct olui_event& ev)
{
    struct olui *ui = OLGetUI();
    if ( ui && ev.index >= 0 && (size_t)ev.index < m_olChecked.size() )
    {
        // a click on the tick (or a double-click, Return) turns it
        olui_rect box;
        int tick = 0, h;
        GetTextExtent("[x] ", &tick, &h);
        const bool onTick = ev.type == OLUI_SELECT && olui_box(ui, m_olEl, &box) && ev.x - box.x < tick + 6;
        if ( onTick || ev.type == OLUI_ACTIVATE )
        {
            Check(ev.index, !m_olChecked[ev.index]);
            wxCommandEvent e(wxEVT_CHECKLISTBOX, GetId());
            InitCommandEventWithItems(e, ev.index);
            HandleWindowEvent(e);
            if ( ev.type == OLUI_ACTIVATE )
                return;
        }
    }
    wxListBox::OLHandleEvent(ev);
}

int wxCheckListBox::DoInsertItems(const wxArrayStringsAdapter& items, unsigned int pos, void **clientData,
                                  wxClientDataType type)
{
    m_olChecked.insert(m_olChecked.begin() + pos, items.GetCount(), false);
    return wxListBox::DoInsertItems(items, pos, clientData, type);
}

void wxCheckListBox::DoClear()
{
    m_olChecked.clear();
    wxListBox::DoClear();
}

void wxCheckListBox::DoDeleteOneItem(unsigned int pos)
{
    m_olChecked.erase(m_olChecked.begin() + pos);
    wxListBox::DoDeleteOneItem(pos);
}

// ---- wxRadioBox --------------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxRadioBox, wxControl);

bool wxRadioBox::Create(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos,
                        const wxSize& size, int n, const wxString choices[], int majorDim, long style,
                        const wxValidator& val, const wxString& name)
{
    m_labelOrig = title;
    for ( int i = 0; i < n; i++ )
        m_olItems.push_back(choices[i]);
    m_olEnabled.assign(n, true);
    m_olShown.assign(n, true);
    m_olSel = n > 0 ? 0 : wxNOT_FOUND;
    SetMajorDim(majorDim == 0 ? n : majorDim, style);
    if ( !wxControl::Create(parent, id, pos, size, style, val, name) )
        return false;
    SetInitialSize(size);
    return true;
}

bool wxRadioBox::Create(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos,
                        const wxSize& size, const wxArrayString& choices, int majorDim, long style,
                        const wxValidator& val, const wxString& name)
{
    wxCArrayString chs(choices);
    return Create(parent, id, title, pos, size, chs.GetCount(), chs.GetStrings(), majorDim, style, val, name);
}

static wxString ItemId(const wxRadioBox *rb, unsigned int n)
{
    return wxString::Format("%s_%u", rb->OLId(), n);
}

wxString wxRadioBox::OLGetMarkup() const
{
    wxString m = "<fieldset id=" + OLId() + DisabledAttr(this) + " style='padding:4px'>";
    if ( !GetLabelText().empty() )
        m += "<legend>" + wxOLEscape(GetLabelText()) + "</legend>";
    // rows of columns, as wx counts them
    const unsigned rows = wxMax(1u, GetRowCount()), cols = wxMax(1u, GetColumnCount());
    m += "<div style='display:flex;flex-direction:column;gap:2px'>";
    for ( unsigned r = 0; r < rows; r++ )
    {
        m += "<div style='display:flex;gap:8px'>";
        for ( unsigned c = 0; c < cols; c++ )
        {
            const unsigned n = HasFlag(wxRA_SPECIFY_ROWS) ? c * rows + r : r * cols + c;
            if ( n >= m_olItems.size() )
                break;
            wxString attrs = " id=" + ItemId(this, n) + " name=" + OLId();
            if ( (int)n == m_olSel )
                attrs += " checked";
            if ( !m_olEnabled[n] )
                attrs += " disabled";
            m += wxString("<label style='display:flex;gap:4px;align-items:center") +
                 (m_olShown[n] ? "" : ";display:none") + "'><radio" + attrs + "></radio>" +
                 wxOLEscape(wxStripMenuCodes(m_olItems[n], wxStrip_Mnemonics)) + "</label>";
        }
        m += "</div>";
    }
    return m + "</div></fieldset>";
}

void wxRadioBox::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CHANGE || !ev.checked )
        return;
    const wxString id = wxOLFromUTF8(ev.id), prefix = OLId() + "_";
    unsigned long n;
    if ( !id.StartsWith(prefix) || !id.Mid(prefix.length()).ToULong(&n) || n >= m_olItems.size() ||
         (int)n == m_olSel )
        return;
    m_olSel = (int)n;
    wxCommandEvent e(wxEVT_RADIOBOX, GetId());
    e.SetInt(m_olSel);
    e.SetString(m_olItems[n]);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

bool wxRadioBox::Enable(unsigned int n, bool enable)
{
    wxCHECK_MSG(n < m_olItems.size(), false, "invalid index");
    if ( m_olEnabled[n] == enable )
        return false;
    m_olEnabled[n] = enable;
    struct olui *ui = OLGetUI();
    if ( struct olui_el *r = m_olEl && ui ? olui_find(ui, ItemId(this, n).utf8_str()) : NULL )
        olui_set_disabled(ui, r, !enable);
    return true;
}

bool wxRadioBox::Enable(bool enable)
{
    return wxControl::Enable(enable);
}

bool wxRadioBox::Show(unsigned int n, bool show)
{
    wxCHECK_MSG(n < m_olItems.size(), false, "invalid index");
    if ( m_olShown[n] == show )
        return false;
    m_olShown[n] = show;
    OLRecreate();
    return true;
}

bool wxRadioBox::Show(bool show)
{
    return wxControl::Show(show);
}

bool wxRadioBox::IsItemEnabled(unsigned int n) const
{
    return n < m_olEnabled.size() && m_olEnabled[n];
}

bool wxRadioBox::IsItemShown(unsigned int n) const
{
    return n < m_olShown.size() && m_olShown[n];
}

void wxRadioBox::SetString(unsigned int n, const wxString& s)
{
    wxCHECK_RET(n < m_olItems.size(), "invalid index");
    m_olItems[n] = s;
    OLRecreate();
}

void wxRadioBox::SetSelection(int n)
{
    wxCHECK_RET(n >= 0 && (size_t)n < m_olItems.size(), "invalid index");
    m_olSel = n;
    struct olui *ui = OLGetUI();
    if ( !m_olEl || !ui )
        return;
    for ( unsigned i = 0; i < m_olItems.size(); i++ )
        if ( struct olui_el *r = olui_find(ui, ItemId(this, i).utf8_str()) )
            olui_set_checked(ui, r, (int)i == n);
}

wxSize wxRadioBox::DoGetBestSize() const
{
    int w, h;
    GetTextExtent("W", &w, &h);
    const unsigned rows = wxMax(1u, GetRowCount()), cols = wxMax(1u, GetColumnCount());
    const int item = Widest(this, m_olItems) + h + 12;
    int title, th;
    GetTextExtent(GetLabelText(), &title, &th);
    return wxSize(wxMax((int)cols * (item + 8), title + 24) + 12, (int)rows * (h + 6) + h + 16);
}

// ---- wxNotebook --------------------------------------------------------------------------------

// (wxNotebook's class info is in wx's common code)

bool wxNotebook::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style,
                        const wxString& name)
{
    if ( (style & wxBK_ALIGN_MASK) == wxBK_DEFAULT )
        style |= wxBK_TOP;
    return wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name);
}

int wxNotebook::OLTabHeight() const
{
    return GetCharHeight() + 10;
}

// the tabs: a box of their own, first in the notebook's, so the pages
// (after it) are drawn over the tabs' page area
static wxString TabsMarkup(const wxNotebook *nb, const wxArrayString& texts, int sel)
{
    const wxSize s = nb->GetSize();
    wxString m = wxString::Format("<tabs id=%st style='position:absolute;left:0;top:0;width:%dpx;height:%dpx'>",
                                  nb->OLId(), wxMax(s.x, 0), wxMax(s.y, 0));
    for ( size_t i = 0; i < texts.size(); i++ )
        m += "<tab title=\"" + wxOLEscape(wxStripMenuCodes(texts[i], wxStrip_Mnemonics)) + "\"" +
             ((int)i == sel ? " selected" : "") + "></tab>";
    return m + "</tabs>";
}

wxString wxNotebook::OLGetMarkup() const
{
    return "<div id=" + OLId() + DisabledAttr(this) + "><div id=" + OLId() + "w>" +
           TabsMarkup(this, m_olTexts, m_selection) + "</div></div>";
}

void wxNotebook::OLElementMade()
{
    OLPlacePages();
}

// the tabs made again (a page put in or taken out, a text changed)
static void RebuildTabs(wxNotebook *nb, const wxArrayString& texts, int sel)
{
    struct olui *ui = nb->OLGetUI();
    if ( !ui || !nb->OLGetElement() )
        return;
    struct olui_el *wrap = olui_find(ui, (nb->OLId() + "w").utf8_str());
    if ( !wrap )
        return;
    while ( struct olui_el *c = olui_first_child(wrap) )
        olui_remove(ui, c);
    char err[96];
    olui_add(ui, wrap, wxOLToUTF8(TabsMarkup(nb, texts, sel)).c_str(), err, sizeof err);
    nb->OLGetTLW()->OLNeedLayout();
}

void wxNotebook::OLPlacePages()
{
    const wxSize s = GetSize();
    const int th = OLTabHeight();
    for ( size_t i = 0; i < m_pages.size(); i++ )
    {
        wxWindow *p = m_pages[i];
        if ( !p )
            continue;
        p->SetSize(2, th + 2, wxMax(s.x - 4, 0), wxMax(s.y - th - 4, 0));
        p->Show((int)i == m_selection);
    }
}

void wxNotebook::DoSetSize(int x, int y, int w, int h, int sizeFlags)
{
    wxNotebookBase::DoSetSize(x, y, w, h, sizeFlags);
    struct olui *ui = OLGetUI();
    if ( struct olui_el *tabs = ui && m_olEl ? olui_find(ui, (OLId() + "t").utf8_str()) : NULL )
    {
        const wxSize s = GetSize();
        olui_set_style(ui, tabs, wxString::Format("position:absolute;left:0;top:0;width:%dpx;height:%dpx",
                                                  wxMax(s.x, 0), wxMax(s.y, 0)).utf8_str());
    }
    OLPlacePages();
}

void wxNotebook::SetPadding(const wxSize& WXUNUSED(padding))
{
}

void wxNotebook::SetTabSize(const wxSize& WXUNUSED(sz))
{
}

bool wxNotebook::SetPageText(size_t n, const wxString& text)
{
    wxCHECK_MSG(n < m_olTexts.size(), false, "invalid page");
    m_olTexts[n] = text;
    RebuildTabs(this, m_olTexts, m_selection);
    return true;
}

wxString wxNotebook::GetPageText(size_t n) const
{
    wxCHECK_MSG(n < m_olTexts.size(), wxString(), "invalid page");
    return m_olTexts[n];
}

int wxNotebook::GetPageImage(size_t n) const
{
    return n < m_olImages.size() ? m_olImages[n] : wxNOT_FOUND;
}

bool wxNotebook::SetPageImage(size_t n, int imageId)
{
    wxCHECK_MSG(n < m_olImages.size(), false, "invalid page");
    m_olImages[n] = imageId;
    return true;
}

bool wxNotebook::InsertPage(size_t n, wxWindow *page, const wxString& text, bool bSelect, int imageId)
{
    if ( !wxNotebookBase::InsertPage(n, page, text, bSelect, imageId) )
        return false;
    m_olTexts.Insert(text, n);
    m_olImages.insert(m_olImages.begin() + n, imageId);
    if ( m_selection != wxNOT_FOUND && (size_t)m_selection >= n )
        m_selection++;
    page->Show(false);
    RebuildTabs(this, m_olTexts, m_selection);
    DoSetSelectionAfterInsertion(n, bSelect);
    OLPlacePages();
    return true;
}

wxSize wxNotebook::CalcSizeFromPage(const wxSize& sizePage) const
{
    return wxSize(sizePage.x + 4, sizePage.y + OLTabHeight() + 4);
}

bool wxNotebook::DeleteAllPages()
{
    if ( !wxNotebookBase::DeleteAllPages() )
        return false;
    m_olTexts.clear();
    m_olImages.clear();
    RebuildTabs(this, m_olTexts, wxNOT_FOUND);
    return true;
}

wxWindow *wxNotebook::DoRemovePage(size_t page)
{
    wxWindow *w = wxNotebookBase::DoRemovePage(page);
    if ( !w )
        return NULL;
    m_olTexts.RemoveAt(page);
    m_olImages.erase(m_olImages.begin() + page);
    DoSetSelectionAfterRemoval(page);
    RebuildTabs(this, m_olTexts, m_selection);
    OLPlacePages();
    return w;
}

int wxNotebook::DoSetSelection(size_t nPage, int flags)
{
    wxCHECK_MSG(nPage < GetPageCount(), wxNOT_FOUND, "invalid page");
    const int old = m_selection;
    if ( (int)nPage == old )
        return old;
    if ( (flags & SetSelection_SendEvent) && !SendPageChangingEvent((int)nPage) )
    {
        // vetoed: the tabs show the old one again
        if ( struct olui_el *tabs = OLGetUI() && m_olEl ? olui_find(OLGetUI(), (OLId() + "t").utf8_str()) : NULL )
            if ( old != wxNOT_FOUND )
                olui_tabs_select(OLGetUI(), tabs, old);
        return old;
    }
    m_selection = (int)nPage;
    if ( struct olui_el *tabs = OLGetUI() && m_olEl ? olui_find(OLGetUI(), (OLId() + "t").utf8_str()) : NULL )
        if ( olui_tabs_current(tabs) != (int)nPage )
            olui_tabs_select(OLGetUI(), tabs, (int)nPage);
    for ( size_t i = 0; i < m_pages.size(); i++ )
        if ( m_pages[i] )
            m_pages[i]->Show(i == nPage);
    if ( flags & SetSelection_SendEvent )
        SendPageChangedEvent(old, (int)nPage);
    return old;
}

void wxNotebook::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type == OLUI_SELECT && ev.index >= 0 && (size_t)ev.index < GetPageCount() )
        DoSetSelection((size_t)ev.index, SetSelection_SendEvent);
}
