/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dataobj.h
// Purpose:     wxDataObject
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DATAOBJ_H_
#define _WX_OPENLAYOUT_DATAOBJ_H_

class WXDLLIMPEXP_CORE wxDataObject : public wxDataObjectBase
{
public:
    wxDataObject() {}

    virtual bool IsSupportedFormat(const wxDataFormat& format, Direction dir = Get) const;
};

#endif // _WX_OPENLAYOUT_DATAOBJ_H_
