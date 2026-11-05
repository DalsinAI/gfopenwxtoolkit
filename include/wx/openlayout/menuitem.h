/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/menuitem.h
// Purpose:     wxMenuItem: an Intuition menu item
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_MENUITEM_H_
#define _WX_OPENLAYOUT_MENUITEM_H_

class WXDLLIMPEXP_CORE wxMenuItem : public wxMenuItemBase
{
public:
    wxMenuItem(wxMenu *parentMenu = NULL, int id = wxID_SEPARATOR,
               const wxString& text = wxEmptyString,
               const wxString& help = wxEmptyString,
               wxItemKind kind = wxITEM_NORMAL,
               wxMenu *subMenu = NULL);

    virtual void SetItemLabel(const wxString& label) wxOVERRIDE;
    virtual void Enable(bool enable = true) wxOVERRIDE;
    virtual void Check(bool check = true) wxOVERRIDE;
    virtual void SetBitmap(const wxBitmapBundle& bitmap) wxOVERRIDE { m_bitmap = bitmap; }
    virtual wxBitmap GetBitmap() const wxOVERRIDE { return m_bitmap.GetBitmap(wxDefaultSize); }

    // the radio group it is in: the first and last item of it
    void OLSetRadioGroup(int start, int end) { m_olRadioStart = start; m_olRadioEnd = end; }
    int OLRadioStart() const { return m_olRadioStart; }
    int OLRadioEnd() const { return m_olRadioEnd; }

private:
    wxBitmapBundle m_bitmap;
    int m_olRadioStart, m_olRadioEnd;

    wxDECLARE_DYNAMIC_CLASS(wxMenuItem);
    wxDECLARE_NO_COPY_CLASS(wxMenuItem);
};

#endif // _WX_OPENLAYOUT_MENUITEM_H_
