/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/anybutton.h
// Purpose:     wxAnyButton: an olui button
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_ANYBUTTON_H_
#define _WX_OPENLAYOUT_ANYBUTTON_H_

class WXDLLIMPEXP_CORE wxAnyButton : public wxAnyButtonBase
{
public:
    wxAnyButton() {}

    virtual void SetLabel(const wxString& label) wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }
    virtual bool OLWantsImage() const wxOVERRIDE { return m_olBitmaps[State_Normal].IsOk(); }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    void OLOnPaint(wxPaintEvent& event);
    virtual wxBitmap DoGetBitmap(State state) const wxOVERRIDE;
    virtual void DoSetBitmap(const wxBitmapBundle& bitmap, State which) wxOVERRIDE;

    wxBitmapBundle m_olBitmaps[State_Max];

    wxDECLARE_NO_COPY_CLASS(wxAnyButton);
};

#endif // _WX_OPENLAYOUT_ANYBUTTON_H_
