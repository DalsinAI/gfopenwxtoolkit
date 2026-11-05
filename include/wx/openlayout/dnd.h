/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dnd.h
// Purpose:     drag and drop within the program (Workbench drops come as AppWindow messages)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DND_H_
#define _WX_OPENLAYOUT_DND_H_

#include "wx/scopedptr.h"

class WXDLLIMPEXP_CORE wxDropTarget : public wxDropTargetBase
{
public:
    wxDropTarget(wxDataObject *dataObject = NULL) : wxDropTargetBase(dataObject) {}

    virtual bool OnDrop(wxCoord x, wxCoord y) wxOVERRIDE;
    virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def) wxOVERRIDE;
    virtual bool GetData() wxOVERRIDE;
    virtual wxDataFormat GetMatchingPair();

    // the port's own: what is being dropped
    void OLSetSource(wxDataObject *source) { m_olSource = source; }

private:
    wxDataObject *m_olSource = NULL;
};

class WXDLLIMPEXP_CORE wxDropSource : public wxDropSourceBase
{
public:
    wxDropSource(wxWindow *win = NULL,
                 const wxCursor& copy = wxNullCursor,
                 const wxCursor& move = wxNullCursor,
                 const wxCursor& none = wxNullCursor);
    wxDropSource(wxDataObject& data, wxWindow *win = NULL,
                 const wxCursor& copy = wxNullCursor,
                 const wxCursor& move = wxNullCursor,
                 const wxCursor& none = wxNullCursor);

    virtual wxDragResult DoDragDrop(int flags = wxDrag_CopyOnly) wxOVERRIDE;

private:
    wxWindow *m_win;
};

#endif // _WX_OPENLAYOUT_DND_H_
