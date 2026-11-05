/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/popupwin.h
// Purpose:     wxPopupWindow: a borderless window of its own
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_POPUPWIN_H_
#define _WX_OPENLAYOUT_POPUPWIN_H_

class WXDLLIMPEXP_CORE wxPopupWindow : public wxPopupWindowBase
{
public:
    wxPopupWindow() {}
    wxPopupWindow(wxWindow *parent, int flags = wxBORDER_NONE) { Create(parent, flags); }

    bool Create(wxWindow *parent, int flags = wxBORDER_NONE);

    virtual unsigned long OLGetFlags() const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxPopupWindow);
};

#endif // _WX_OPENLAYOUT_POPUPWIN_H_
