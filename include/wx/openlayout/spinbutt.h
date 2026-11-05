/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/spinbutt.h
// Purpose:     wxSpinButton: two arrows wx draws
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_SPINBUTT_H_
#define _WX_OPENLAYOUT_SPINBUTT_H_

#include "wx/timer.h"

class WXDLLIMPEXP_CORE wxSpinButton : public wxSpinButtonBase
{
public:
    wxSpinButton() : m_olValue(0), m_olHeld(0){}
    wxSpinButton(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSP_VERTICAL, const wxString& name = wxSPIN_BUTTON_NAME) : m_olValue(0), m_olHeld(0)
    {
        Create(parent, id, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSP_VERTICAL, const wxString& name = wxSPIN_BUTTON_NAME);

    virtual int GetValue() const wxOVERRIDE { return m_olValue; }
    virtual void SetValue(int val) wxOVERRIDE;
    virtual void SetRange(int min, int max) wxOVERRIDE;
    virtual bool AcceptsFocus() const wxOVERRIDE { return false; }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
private:
    void OnPaint(wxPaintEvent& event);
    void OnMouse(wxMouseEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OLStep(int dir);
    int m_olValue;
    int m_olHeld;                   // the arrow held: 1 up, -1 down
    wxTimer m_olRepeat;
public:

    wxDECLARE_DYNAMIC_CLASS(wxSpinButton);
    wxDECLARE_NO_COPY_CLASS(wxSpinButton);
};

#endif // _WX_OPENLAYOUT_SPINBUTT_H_
