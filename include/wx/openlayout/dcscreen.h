/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dcscreen.h
// Purpose:     wxScreenDC: the screen's RastPort
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DCSCREEN_H_
#define _WX_OPENLAYOUT_DCSCREEN_H_

#include "wx/openlayout/dc.h"

class WXDLLIMPEXP_CORE wxScreenDCImpl : public wxOLDCImpl
{
public:
    wxScreenDCImpl(wxScreenDC *owner);

    wxDECLARE_ABSTRACT_CLASS(wxScreenDCImpl);
};

#endif // _WX_OPENLAYOUT_DCSCREEN_H_
