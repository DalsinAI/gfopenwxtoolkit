/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/tooltip.h
// Purpose:     wxToolTip: GFOpenLayout's tooltips
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TOOLTIP_H_
#define _WX_OPENLAYOUT_TOOLTIP_H_

class WXDLLIMPEXP_FWD_CORE wxWindow;

class WXDLLIMPEXP_CORE wxToolTip : public wxObject
{
public:
    wxToolTip(const wxString& tip) : m_text(tip), m_window(NULL) {}

    static void Enable(bool flag);
    static void SetDelay(long milliseconds);
    static void SetAutoPop(long milliseconds);
    static void SetReshow(long milliseconds);
    static void SetMaxWidth(int width);

    void SetTip(const wxString& tip);
    const wxString& GetTip() const { return m_text; }

    void SetWindow(wxWindow *win) { m_window = win; }
    wxWindow *GetWindow() const { return m_window; }

private:
    wxString m_text;
    wxWindow *m_window;

    wxDECLARE_ABSTRACT_CLASS(wxToolTip);
};

#endif // _WX_OPENLAYOUT_TOOLTIP_H_
