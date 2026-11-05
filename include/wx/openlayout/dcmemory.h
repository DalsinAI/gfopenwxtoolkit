/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dcmemory.h
// Purpose:     wxMemoryDC: drawing on a bitmap's BitMap
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DCMEMORY_H_
#define _WX_OPENLAYOUT_DCMEMORY_H_

#include "wx/openlayout/dc.h"
#include "wx/dcmemory.h"

class WXDLLIMPEXP_CORE wxMemoryDCImpl : public wxOLDCImpl
{
public:
    wxMemoryDCImpl(wxMemoryDC *owner);
    wxMemoryDCImpl(wxMemoryDC *owner, wxBitmap& bitmap);
    wxMemoryDCImpl(wxMemoryDC *owner, wxDC *dc);
    virtual ~wxMemoryDCImpl();

    virtual void DoSelect(const wxBitmap& bitmap) wxOVERRIDE;
    virtual const wxBitmap& GetSelectedBitmap() const wxOVERRIDE { return m_bmp; }
    virtual wxBitmap& GetSelectedBitmap() wxOVERRIDE { return m_bmp; }

private:
    void Init();
    void OLRelease();

    wxBitmap m_bmp;
    struct RastPort *m_olRP;

    wxDECLARE_ABSTRACT_CLASS(wxMemoryDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxMemoryDCImpl);
};

#endif // _WX_OPENLAYOUT_DCMEMORY_H_
