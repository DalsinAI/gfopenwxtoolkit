/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/scrolbar.h
// Purpose:     wxScrollBar: a bar wx draws, its arrows, track and thumb
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_SCROLBAR_H_
#define _WX_OPENLAYOUT_SCROLBAR_H_

#include "wx/timer.h"

class WXDLLIMPEXP_CORE wxScrollBar : public wxScrollBarBase
{
public:
    wxScrollBar() : m_olPos(0), m_olThumb(1), m_olRange(1), m_olPage(1), m_olDrag(-1), m_olDragFrom(0), m_olHeld(0){}
    wxScrollBar(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSB_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxScrollBarNameStr)) : m_olPos(0), m_olThumb(1), m_olRange(1), m_olPage(1), m_olDrag(-1), m_olDragFrom(0), m_olHeld(0)
    {
        Create(parent, id, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSB_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxScrollBarNameStr));

    virtual int GetThumbPosition() const wxOVERRIDE { return m_olPos; }
    virtual int GetThumbSize() const wxOVERRIDE { return m_olThumb; }
    virtual int GetPageSize() const wxOVERRIDE { return m_olPage; }
    virtual int GetRange() const wxOVERRIDE { return m_olRange; }
    virtual void SetThumbPosition(int viewStart) wxOVERRIDE;
    virtual void SetScrollbar(int position, int thumbSize, int range, int pageSize,
                              bool refresh = true) wxOVERRIDE;
    virtual bool AcceptsFocus() const wxOVERRIDE { return false; }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
private:
    void OnPaint(wxPaintEvent& event);
    void OnMouse(wxMouseEvent& event);
    void OnTimer(wxTimerEvent& event);
    // the track's pixels: where the thumb is and how long
    void OLThumb(int *at, int *len, int *track, int *arrow) const;
    void OLScrollTo(int pos, wxEventType type);
    int m_olPos, m_olThumb, m_olRange, m_olPage;
    int m_olDrag, m_olDragFrom;     // the thumb being dragged: where it was taken
    int m_olHeld;                   // an arrow or the track held: -1 back, 1 on, -2, 2 a page
    wxTimer m_olRepeat;
public:

    wxDECLARE_DYNAMIC_CLASS(wxScrollBar);
    wxDECLARE_NO_COPY_CLASS(wxScrollBar);
};

#endif // _WX_OPENLAYOUT_SCROLBAR_H_
