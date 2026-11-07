/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/menu.cpp
// Purpose:     wxMenu, wxMenuBar and wxMenuItem: a frame's menu bar is an
//              Intuition menu strip made with GadTools (the screen's title
//              bar, the right button); a pop-up menu is an olui menu
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_MENUS

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/frame.h"
    #include "wx/nonownedwnd.h"
    #include "wx/log.h"
#endif

#include "wx/evtloop.h"

#include <vector>

#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include "private.h"

// gadtools.library, through our own base (CreateMenusA, FreeMenus,
// LayoutMenusA, GetVisualInfoA, FreeVisualInfo)
static struct Library *gs_gadToolsBase = NULL;
#define wxOL_CreateMenusA(nm, tags) LP2(0x30, struct Menu *, CreateMenusA, const struct NewMenu *, (nm), a0, \
    const struct TagItem *, (tags), a1, , gs_gadToolsBase)
#define wxOL_FreeMenus(m) LP1NR(0x36, FreeMenus, struct Menu *, (m), a0, , gs_gadToolsBase)
#define wxOL_LayoutMenusA(m, vi, tags) LP3(0x42, BOOL, LayoutMenusA, struct Menu *, (m), a0, APTR, (vi), a1, \
    const struct TagItem *, (tags), a2, , gs_gadToolsBase)
#define wxOL_GetVisualInfoA(scr, tags) LP2(0x7e, APTR, GetVisualInfoA, struct Screen *, (scr), a0, \
    const struct TagItem *, (tags), a1, , gs_gadToolsBase)
#define wxOL_FreeVisualInfo(vi) LP1NR(0x84, FreeVisualInfo, APTR, (vi), a0, , gs_gadToolsBase)

static bool OpenGadTools()
{
    if ( !gs_gadToolsBase )
        gs_gadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39);
    return gs_gadToolsBase != NULL;
}

// Intuition's limits: menus in a strip, items in a menu, items in a sub-menu
static const int MAX_MENUS = 31, MAX_ITEMS = 63, MAX_SUBITEMS = 31;

// ---- running an item --------------------------------------------------------------------

// an item chosen (the strip, a key, a pop-up): checked for a check item is
// its new state (-1: turned), and the menu's event sent
void wxOLRunMenuItem(wxMenuItem *item, int checked)
{
    wxMenu *menu = item->GetMenu();
    if ( !menu || !item->IsEnabled() )
        return;
    int state = -1;
    if ( item->IsRadio() )
    {
        item->Check(true);
        state = 1;
    }
    else if ( item->IsCheckable() )
    {
        item->Check(checked < 0 ? !item->IsChecked() : checked != 0);
        state = item->IsChecked();
    }
    menu->SendEvent(item->GetId(), state);
}

// ---- wxMenuItem ---------------------------------------------------------------------------

// (wxMenuItem's class info is in wx's common code)

wxMenuItem *wxMenuItemBase::New(wxMenu *parentMenu, int itemid, const wxString& text, const wxString& help,
                                wxItemKind kind, wxMenu *subMenu)
{
    return new wxMenuItem(parentMenu, itemid, text, help, kind, subMenu);
}

wxMenuItem::wxMenuItem(wxMenu *parentMenu, int id, const wxString& text, const wxString& help,
                       wxItemKind kind, wxMenu *subMenu)
    : wxMenuItemBase(parentMenu, id, text, help, kind, subMenu), m_olRadioStart(-1), m_olRadioEnd(-1)
{
}

void wxMenuItem::SetItemLabel(const wxString& label)
{
    wxMenuItemBase::SetItemLabel(label);
    if ( m_parentMenu )
        m_parentMenu->OLChanged(true);
}

void wxMenuItem::Enable(bool enable)
{
    if ( enable == IsEnabled() )
        return;
    wxMenuItemBase::Enable(enable);
    if ( m_parentMenu )
        m_parentMenu->OLChanged(false);
}

void wxMenuItem::Check(bool check)
{
    wxCHECK_RET(IsCheckable(), "only checkable items may be checked");
    if ( check == IsChecked() )
        return;
    if ( IsRadio() )
    {
        if ( !check )
            return;     // a radio item goes off when another of its group goes on
        // the others of its group off
        if ( m_parentMenu && m_olRadioStart >= 0 )
        {
            const wxMenuItemList& items = m_parentMenu->GetMenuItems();
            int pos = 0;
            for ( wxMenuItemList::compatibility_iterator n = items.GetFirst(); n; n = n->GetNext(), pos++ )
                if ( pos >= m_olRadioStart && pos <= m_olRadioEnd && n->GetData() != this )
                    n->GetData()->wxMenuItemBase::Check(false);
        }
    }
    wxMenuItemBase::Check(check);
    if ( m_parentMenu )
        m_parentMenu->OLChanged(false);
}

// ---- wxMenu ---------------------------------------------------------------------------------

// (wxMenu's class info is in wx's common code)

wxMenu::~wxMenu()
{
}

void wxMenu::OLChanged(bool structure)
{
    if ( wxMenuBar *mb = GetMenuBar() )
        mb->OLChanged(structure);
}

void wxMenu::OLUpdateRadioGroups()
{
    // runs of radio items: each item knows its run's first and last place
    const wxMenuItemList& items = GetMenuItems();
    std::vector<wxMenuItem *> list;
    for ( wxMenuItemList::compatibility_iterator n = items.GetFirst(); n; n = n->GetNext() )
        list.push_back(n->GetData());
    size_t i = 0;
    while ( i < list.size() )
    {
        if ( !list[i]->IsRadio() )
        {
            list[i]->OLSetRadioGroup(-1, -1);
            i++;
            continue;
        }
        size_t end = i;
        while ( end + 1 < list.size() && list[end + 1]->IsRadio() )
            end++;
        bool any = false;
        for ( size_t k = i; k <= end; k++ )
        {
            list[k]->OLSetRadioGroup((int)i, (int)end);
            any = any || list[k]->IsChecked();
        }
        if ( !any )
            list[i]->wxMenuItemBase::Check(true);   // a group's first is on
        i = end + 1;
    }
}

wxMenuItem *wxMenu::DoAppend(wxMenuItem *item)
{
    if ( !wxMenuBase::DoAppend(item) )
        return NULL;
    OLUpdateRadioGroups();
    OLChanged(true);
    return item;
}

wxMenuItem *wxMenu::DoInsert(size_t pos, wxMenuItem *item)
{
    if ( !wxMenuBase::DoInsert(pos, item) )
        return NULL;
    OLUpdateRadioGroups();
    OLChanged(true);
    return item;
}

wxMenuItem *wxMenu::DoRemove(wxMenuItem *item)
{
    wxMenuItem *r = wxMenuBase::DoRemove(item);
    OLUpdateRadioGroups();
    OLChanged(true);
    return r;
}

// ---- wxMenuBar -------------------------------------------------------------------------------

// (wxMenuBar's class info is in wx's common code)

void wxMenuBar::Init()
{
    m_olStrip = NULL;
    m_olVisual = NULL;
    m_olNewMenu = NULL;
    m_olDirty = 2;
}

wxMenuBar::wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long WXUNUSED(style))
{
    Init();
    for ( size_t i = 0; i < n; i++ )
        Append(menus[i], titles[i]);
}

wxMenuBar::~wxMenuBar()
{
    if ( m_menuBarFrame )
    {
        struct Window *win = m_menuBarFrame->OLGetWindow();
        if ( win && win->MenuStrip == m_olStrip && m_olStrip )
            ClearMenuStrip(win);
    }
    OLFreeStrip();
}

void wxMenuBar::OLFreeStrip()
{
    if ( m_olStrip )
        wxOL_FreeMenus(m_olStrip);
    m_olStrip = NULL;
    if ( m_olVisual )
        wxOL_FreeVisualInfo(m_olVisual);
    m_olVisual = NULL;
    free(m_olNewMenu);
    m_olNewMenu = NULL;
    m_olItems.clear();
}

void wxMenuBar::OLChanged(bool structure)
{
    const int level = structure ? 2 : 1;
    if ( level > m_olDirty )
        m_olDirty = level;
    wxOLWakeMain();
}

bool wxMenuBar::Append(wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Append(menu, title) )
        return false;
    m_olTitles.push_back(title);
    m_olTopEnabled.push_back(true);
    OLChanged(true);
    return true;
}

bool wxMenuBar::Insert(size_t pos, wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Insert(pos, menu, title) )
        return false;
    m_olTitles.insert(m_olTitles.begin() + pos, title);
    m_olTopEnabled.insert(m_olTopEnabled.begin() + pos, true);
    OLChanged(true);
    return true;
}

wxMenu *wxMenuBar::Replace(size_t pos, wxMenu *menu, const wxString& title)
{
    wxMenu *old = wxMenuBarBase::Replace(pos, menu, title);
    if ( old && pos < m_olTitles.size() )
    {
        m_olTitles[pos] = title;
        OLChanged(true);
    }
    return old;
}

wxMenu *wxMenuBar::Remove(size_t pos)
{
    // the strip points into the menu: it goes first
    if ( m_menuBarFrame )
    {
        struct Window *win = m_menuBarFrame->OLGetWindow();
        if ( win && win->MenuStrip == m_olStrip && m_olStrip )
            ClearMenuStrip(win);
    }
    OLFreeStrip();
    wxMenu *menu = wxMenuBarBase::Remove(pos);
    if ( menu && pos < m_olTitles.size() )
    {
        m_olTitles.erase(m_olTitles.begin() + pos);
        m_olTopEnabled.erase(m_olTopEnabled.begin() + pos);
    }
    OLChanged(true);
    return menu;
}

void wxMenuBar::EnableTop(size_t pos, bool enable)
{
    wxCHECK_RET(pos < m_olTopEnabled.size(), "invalid menu");
    m_olTopEnabled[pos] = enable;
    OLChanged(false);
}

bool wxMenuBar::IsEnabledTop(size_t pos) const
{
    return pos < m_olTopEnabled.size() && m_olTopEnabled[pos];
}

void wxMenuBar::SetMenuLabel(size_t pos, const wxString& label)
{
    wxCHECK_RET(pos < m_olTitles.size(), "invalid menu");
    m_olTitles[pos] = label;
    OLChanged(true);
}

wxString wxMenuBar::GetMenuLabel(size_t pos) const
{
    wxCHECK_MSG(pos < m_olTitles.size(), wxString(), "invalid menu");
    return m_olTitles[pos];
}

void wxMenuBar::Attach(wxFrame *frame)
{
    wxMenuBarBase::Attach(frame);
    OLChanged(true);
}

void wxMenuBar::Detach()
{
    if ( m_menuBarFrame )
    {
        struct Window *win = m_menuBarFrame->OLGetWindow();
        if ( win && win->MenuStrip == m_olStrip && m_olStrip )
            ClearMenuStrip(win);
    }
    OLFreeStrip();
    m_olDirty = 2;
    wxMenuBarBase::Detach();
}

namespace
{
// a strip being made: the NewMenu entries and their labels
struct StripMaker
{
    std::vector<struct NewMenu> nm;
    std::vector<std::string> labels, keys;
    std::vector<int> labelOf, keyOf;       // per entry: its label's and key's index, -1: none

    void Add(UBYTE type, const std::string& label, const std::string& key, UWORD flags, LONG exclude, APTR user,
             bool bar = false)
    {
        struct NewMenu e;
        memset(&e, 0, sizeof e);
        e.nm_Type = type;
        e.nm_Flags = flags;
        e.nm_MutualExclude = exclude;
        e.nm_UserData = user;
        nm.push_back(e);
        if ( bar )
            labelOf.push_back(-2);
        else
        {
            labelOf.push_back((int)labels.size());
            labels.push_back(label);
        }
        if ( key.empty() )
            keyOf.push_back(-1);
        else
        {
            keyOf.push_back((int)keys.size());
            keys.push_back(key);
        }
    }

    // a menu's items, as items (NM_ITEM) or a sub-menu's (NM_SUB)
    void Items(wxMenu *menu, UBYTE type)
    {
        const int max = type == NM_ITEM ? MAX_ITEMS : MAX_SUBITEMS;
        int k = 0;
        std::vector<wxMenuItem *> list;
        for ( wxMenuItemList::compatibility_iterator n = menu->GetMenuItems().GetFirst(); n; n = n->GetNext() )
            list.push_back(n->GetData());
        for ( size_t i = 0; i < list.size() && k < max; i++ )
        {
            wxMenuItem *item = list[i];
            if ( item->IsSeparator() )
            {
                Add(type, std::string(), std::string(), 0, 0, NULL, true);
                k++;
                continue;
            }
            std::string label = wxOLToLatin1(item->GetItemLabelText());
            std::string key;
#if wxUSE_ACCEL
            if ( wxAcceleratorEntry *a = item->GetAccel() )
            {
                // Ctrl with a letter or digit: the Amiga key's shortcut;
                // the others are written after the label
                const int code = a->GetKeyCode();
                if ( (a->GetFlags() & ~wxACCEL_RAW_CTRL) == wxACCEL_CTRL && code < 128 && isalnum(code) )
                    key = std::string(1, (char)toupper(code));
                else
                    label += "   " + wxOLToLatin1(a->ToString());
                delete a;
            }
#endif
            if ( item->GetSubMenu() )
            {
                if ( type == NM_ITEM )
                {
                    Add(NM_ITEM, label + " \xbb", std::string(), item->IsEnabled() ? 0 : NM_ITEMDISABLED, 0, item);
                    Items(item->GetSubMenu(), NM_SUB);
                }
                else
                    Items(item->GetSubMenu(), NM_SUB);  // deeper: in the sub-menu itself
                k++;
                continue;
            }
            UWORD flags = item->IsEnabled() ? 0 : NM_ITEMDISABLED;
            LONG exclude = 0;
            if ( item->IsRadio() )
            {
                flags |= CHECKIT;
                for ( int g = item->OLRadioStart(); g >= 0 && g <= item->OLRadioEnd(); g++ )
                {
                    const int place = k + (g - (int)i);
                    if ( g != (int)i && place >= 0 && place < 32 )
                        exclude |= 1L << place;
                }
            }
            else if ( item->IsCheckable() )
                flags |= CHECKIT | MENUTOGGLE;
            if ( item->IsCheckable() && item->IsChecked() )
                flags |= CHECKED;
            Add(type, label, key, flags, exclude, item);
            k++;
        }
    }
};
}

struct Menu *wxMenuBar::OLGetStrip(struct Window *win)
{
    if ( !win || !OpenGadTools() )
        return NULL;
    if ( m_olStrip && m_olDirty == 1 )
    {
        // the items' states only: set in the strip as it is
        int mi = 0;
        for ( struct Menu *m = m_olStrip; m; m = m->NextMenu, mi++ )
        {
            if ( (size_t)mi < m_olTopEnabled.size() && m_olTopEnabled[mi] )
                m->Flags |= MENUENABLED;
            else
                m->Flags &= ~MENUENABLED;
            for ( struct MenuItem *it = m->FirstItem; it; it = it->NextItem )
            {
                for ( struct MenuItem *sub = it; sub; sub = sub == it ? it->SubItem : sub->NextItem )
                {
                    wxMenuItem *item = (wxMenuItem *)GTMENUITEM_USERDATA(sub);
                    if ( item && !item->IsSeparator() )
                    {
                        if ( item->IsEnabled() )
                            sub->Flags |= ITEMENABLED;
                        else
                            sub->Flags &= ~ITEMENABLED;
                        if ( item->IsCheckable() )
                        {
                            if ( item->IsChecked() )
                                sub->Flags |= CHECKED;
                            else
                                sub->Flags &= ~CHECKED;
                        }
                    }
                }
            }
        }
        m_olDirty = 0;
        return m_olStrip;
    }
    if ( m_olStrip && m_olDirty == 0 )
        return m_olStrip;

    OLFreeStrip();
    m_olDirty = 0;
    if ( !(m_olVisual = wxOL_GetVisualInfoA(win->WScreen, NULL)) )
        return NULL;

    StripMaker sm;
    int mi = 0;
    for ( wxMenuList::compatibility_iterator n = m_menus.GetFirst(); n && mi < MAX_MENUS; n = n->GetNext(), mi++ )
    {
        sm.Add(NM_TITLE, wxOLToLatin1(wxStripMenuCodes(m_olTitles[mi], wxStrip_Mnemonics)), std::string(),
               m_olTopEnabled[mi] ? 0 : NM_MENUDISABLED, 0, NULL);
        sm.Items(n->GetData(), NM_ITEM);
    }
    sm.Add(NM_END, std::string(), std::string(), 0, 0, NULL);

    // every label and key in one buffer: the strip points into it
    size_t total = 0;
    for ( size_t i = 0; i < sm.labels.size(); i++ )
        total += sm.labels[i].size() + 1;
    for ( size_t i = 0; i < sm.keys.size(); i++ )
        total += sm.keys[i].size() + 1;
    m_olText = wxCharBuffer(total + 1);
    std::vector<char *> labelAt(sm.labels.size()), keyAt(sm.keys.size());
    char *p = m_olText.data();
    for ( size_t i = 0; i < sm.labels.size(); i++ )
    {
        labelAt[i] = p;
        memcpy(p, sm.labels[i].c_str(), sm.labels[i].size() + 1);
        p += sm.labels[i].size() + 1;
    }
    for ( size_t i = 0; i < sm.keys.size(); i++ )
    {
        keyAt[i] = p;
        memcpy(p, sm.keys[i].c_str(), sm.keys[i].size() + 1);
        p += sm.keys[i].size() + 1;
    }
    struct NewMenu *nm = (struct NewMenu *)calloc(sm.nm.size(), sizeof(struct NewMenu));
    if ( !nm )
        return NULL;
    for ( size_t i = 0; i < sm.nm.size(); i++ )
    {
        nm[i] = sm.nm[i];
        if ( sm.nm[i].nm_Type == NM_END )
            continue;
        nm[i].nm_Label = sm.labelOf[i] == -2 ? (STRPTR)NM_BARLABEL : (STRPTR)labelAt[sm.labelOf[i]];
        nm[i].nm_CommKey = sm.keyOf[i] >= 0 ? (STRPTR)keyAt[sm.keyOf[i]] : NULL;
        if ( sm.nm[i].nm_UserData )
            m_olItems.push_back((wxMenuItem *)sm.nm[i].nm_UserData);
    }
    m_olNewMenu = nm;

    struct TagItem none[] = { { TAG_END, 0 } };
    m_olStrip = wxOL_CreateMenusA(nm, none);
    if ( m_olStrip )
    {
        struct TagItem look[] = { { GTMN_NewLookMenus, TRUE }, { TAG_END, 0 } };
        if ( !wxOL_LayoutMenusA(m_olStrip, m_olVisual, look) )
        {
            wxLogDebug("the menus are too big for the screen");
            wxOL_FreeMenus(m_olStrip);
            m_olStrip = NULL;
        }
    }
    return m_olStrip;
}

bool wxMenuBar::OLPicked(unsigned short code)
{
    if ( !m_olStrip )
        return false;
    // Intuition's multi-select: the items picked, before any runs (an item
    // run may make the strip again)
    std::vector<std::pair<wxMenuItem *, int> > picked;
    while ( code != MENUNULL )
    {
        struct MenuItem *it = ItemAddress(m_olStrip, code);
        if ( !it )
            break;
        if ( wxMenuItem *item = (wxMenuItem *)GTMENUITEM_USERDATA(it) )
            picked.push_back(std::make_pair(item, (it->Flags & CHECKED) ? 1 : 0));
        code = it->NextSelect;
    }
    for ( size_t i = 0; i < picked.size(); i++ )
        if ( !picked[i].first->GetSubMenu() )
            wxOLRunMenuItem(picked[i].first, picked[i].first->IsCheckable() ? picked[i].second : -1);
    return !picked.empty();
}

// ---- pop-up menus: an olui menu, shown until one is chosen or it closes ---------------------------------

namespace
{
struct Popup
{
    olui_el *el;
    wxNonOwnedWindow *tlw;
    std::vector<wxMenuItem *> items;
    int chosen;
    bool done;
};
Popup *gs_popup = NULL;

void PopupItems(wxMenu *menu, std::vector<wxMenuItem *>& items, wxString& m)
{
    for ( wxMenuItemList::compatibility_iterator n = menu->GetMenuItems().GetFirst(); n; n = n->GetNext() )
    {
        wxMenuItem *item = n->GetData();
        if ( item->IsSeparator() )
        {
            m += "<option separator></option>";
            continue;
        }
        wxString label = item->GetItemLabelText();
        if ( item->GetSubMenu() )
        {
            m += "<optgroup label=\"" + wxOLEscape(label) + "\">";
            PopupItems(item->GetSubMenu(), items, m);
            m += "</optgroup>";
            continue;
        }
        if ( item->IsCheckable() )
            label = (item->IsChecked() ? "* " : "   ") + label;
#if wxUSE_ACCEL
        if ( wxAcceleratorEntry *a = item->GetAccel() )
        {
            label += "   " + a->ToString();
            delete a;
        }
#endif
        m += wxString::Format("<option value=i%u%s>", (unsigned)items.size(), item->IsEnabled() ? "" : " disabled") +
             wxOLEscape(label) + "</option>";
        items.push_back(item);
    }
}
}

bool wxOLPopupEvent(wxNonOwnedWindow *tlw, const struct olui_event& ev)
{
    if ( !gs_popup || gs_popup->tlw != tlw || ev.el != gs_popup->el )
        return false;
    if ( ev.type == OLUI_ACTIVATE && ev.value && ev.value[0] == 'i' )
    {
        gs_popup->chosen = atoi(ev.value + 1);
        gs_popup->done = true;
    }
    return true;
}

bool wxOLPopupMenu(wxWindow *win, wxMenu *menu, int x, int y)
{
    wxNonOwnedWindow *tlw = win->OLGetTLW();
    struct olui *ui = win->OLGetUI();
    if ( !tlw || !ui || !tlw->OLGetWindow() || gs_popup )
        return false;

    Popup popup;
    popup.tlw = tlw;
    popup.chosen = -1;
    popup.done = false;
    wxString m = "<menu>";
    PopupItems(menu, popup.items, m);
    m += "</menu>";
    char err[96];
    popup.el = olui_add(ui, NULL, wxOLToUTF8(m).c_str(), err, sizeof err);
    if ( !popup.el )
    {
        wxLogDebug("pop-up menu: %s", err);
        return false;
    }
    const wxRect box = win->OLGetBoxInTLW();
    if ( !olui_menu_open(ui, popup.el, box.x + x, box.y + y) )
    {
        olui_remove(ui, popup.el);
        return false;
    }
    gs_popup = &popup;
    tlw->OLDrawPending();

    // until it is chosen from or closed (or its window goes)
    {
        wxGUIEventLoop loop;
        wxEventLoopActivator activate(&loop);
        while ( !popup.done && tlw->OLGetWindow() && olui_popup(ui) == popup.el )
            loop.DispatchTimeout(200);
    }
    gs_popup = NULL;
    if ( tlw->OLGetWindow() )
    {
        if ( olui_popup(ui) == popup.el )
            olui_select_open(ui, popup.el, 0);
        olui_remove(ui, popup.el);
        tlw->OLNeedLayout();
    }
    if ( popup.chosen >= 0 && (size_t)popup.chosen < popup.items.size() )
        wxOLRunMenuItem(popup.items[popup.chosen], -1);
    return true;
}

void wxOLCloseGadTools()
{
    if ( gs_gadToolsBase )
        CloseLibrary(gs_gadToolsBase);
    gs_gadToolsBase = NULL;
}

#endif // wxUSE_MENUS
