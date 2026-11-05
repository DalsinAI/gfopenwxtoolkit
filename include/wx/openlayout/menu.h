/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/menu.h
// Purpose:     wxMenu and wxMenuBar: GadTools menu strips
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_MENU_H_
#define _WX_OPENLAYOUT_MENU_H_

#include "wx/openlayout/ndktypes.h"


// A menu bar is built into a GadTools menu strip (CreateMenusA,
// LayoutMenusA) for its frame's Intuition window each time it changes; a
// pick comes back as the item's wx id. A wxMenu popped up on its own
// (PopupMenu) is a GFOpenLayout context menu.
class WXDLLIMPEXP_CORE wxMenu : public wxMenuBase
{
public:
    wxMenu(const wxString& title, long style = 0) : wxMenuBase(title, style) {}
    wxMenu(long style = 0) : wxMenuBase(style) {}

    virtual ~wxMenu();

    // tells the menu bar it is in that its strip is out of date: its items
    // (structure) or just their states
    void OLChanged(bool structure = true);

protected:
    virtual wxMenuItem *DoAppend(wxMenuItem *item) wxOVERRIDE;
    virtual wxMenuItem *DoInsert(size_t pos, wxMenuItem *item) wxOVERRIDE;
    virtual wxMenuItem *DoRemove(wxMenuItem *item) wxOVERRIDE;

private:
    void OLUpdateRadioGroups();

    wxDECLARE_DYNAMIC_CLASS(wxMenu);
};

class WXDLLIMPEXP_CORE wxMenuBar : public wxMenuBarBase
{
public:
    wxMenuBar() { Init(); }
    wxMenuBar(long style) { (void)style; Init(); }
    wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style = 0);
    virtual ~wxMenuBar();

    virtual bool Append(wxMenu *menu, const wxString& title) wxOVERRIDE;
    virtual bool Insert(size_t pos, wxMenu *menu, const wxString& title) wxOVERRIDE;
    virtual wxMenu *Replace(size_t pos, wxMenu *menu, const wxString& title) wxOVERRIDE;
    virtual wxMenu *Remove(size_t pos) wxOVERRIDE;

    virtual void EnableTop(size_t pos, bool enable) wxOVERRIDE;
    virtual bool IsEnabledTop(size_t pos) const wxOVERRIDE;
    virtual void SetMenuLabel(size_t pos, const wxString& label) wxOVERRIDE;
    virtual wxString GetMenuLabel(size_t pos) const wxOVERRIDE;

    virtual void Attach(wxFrame *frame) wxOVERRIDE;
    virtual void Detach() wxOVERRIDE;

    // the strip for its frame's window (made when it changed), NULL: none
    wxOL_Menu *OLGetStrip(wxOL_Window *win);
    // a pick (IDCMP_MENUPICK's code): the item's command event, for each
    // item picked (Intuition's multi-select); true when one was
    bool OLPicked(unsigned short code);
    // something in it changed: the strip is built again (structure) or its
    // items' states set again, when no menu shows (wxFrame::OLUpdateMenus)
    void OLChanged(bool structure = true);
    bool OLDirty() const { return m_olDirty != 0; }

private:
    void Init();
    void OLFreeStrip();

    wxOL_Menu *m_olStrip;
    void *m_olVisual;
    void *m_olNewMenu;      // the NewMenu array the strip was made from
    wxVector<wxMenuItem*> m_olItems;   // the user data of each NewMenu item
    wxVector<wxString> m_olTitles;
    wxVector<bool> m_olTopEnabled;
    wxCharBuffer m_olText;              // every label, Latin-1, one after another
    int m_olDirty;                      // 0: the strip is right; 1: states; 2: items

    wxDECLARE_DYNAMIC_CLASS(wxMenuBar);
};

#endif // _WX_OPENLAYOUT_MENU_H_
