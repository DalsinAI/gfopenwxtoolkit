/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dcclient.h
// Purpose:     the window DCs
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DCCLIENT_H_
#define _WX_OPENLAYOUT_DCCLIENT_H_

#include "wx/openlayout/dc.h"

class WXDLLIMPEXP_CORE wxWindowDCImpl : public wxOLDCImpl
{
public:
    wxWindowDCImpl(wxDC *owner) : wxOLDCImpl(owner) {}
    wxWindowDCImpl(wxDC *owner, wxWindow *win);
    virtual ~wxWindowDCImpl();

protected:
    // draws into the TLW's olui buffer where win is, the buffer copied to
    // the window when the DC goes (a client DC outside a paint)
    void OLInitForWindow(wxWindow *win, bool clientOnly);

    bool m_olShowAfter;
    wxRect m_olBox;             // win's box in the buffer

    wxDECLARE_ABSTRACT_CLASS(wxWindowDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxWindowDCImpl);
};

class WXDLLIMPEXP_CORE wxClientDCImpl : public wxWindowDCImpl
{
public:
    wxClientDCImpl(wxDC *owner) : wxWindowDCImpl(owner) {}
    wxClientDCImpl(wxDC *owner, wxWindow *win);

    wxDECLARE_ABSTRACT_CLASS(wxClientDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxClientDCImpl);
};

class WXDLLIMPEXP_CORE wxPaintDCImpl : public wxClientDCImpl
{
public:
    wxPaintDCImpl(wxDC *owner) : wxClientDCImpl(owner) {}
    wxPaintDCImpl(wxDC *owner, wxWindow *win);

    wxDECLARE_ABSTRACT_CLASS(wxPaintDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxPaintDCImpl);
};

#endif // _WX_OPENLAYOUT_DCCLIENT_H_
