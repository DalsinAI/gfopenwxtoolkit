/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/textctrl.h
// Purpose:     wxTextCtrl: an olui input, or a textarea when multi-line
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TEXTCTRL_H_
#define _WX_OPENLAYOUT_TEXTCTRL_H_

class WXDLLIMPEXP_CORE wxTextCtrl : public wxTextCtrlBase
{
public:
    wxTextCtrl() : m_olModified(false), m_olMaxLength(0) {}
    wxTextCtrl(wxWindow *parent, wxWindowID id, const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
               long style = 0, const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxASCII_STR(wxTextCtrlNameStr))
        : m_olModified(false), m_olMaxLength(0)
    {
        Create(parent, id, value, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                long style = 0, const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxASCII_STR(wxTextCtrlNameStr));

    virtual int GetLineLength(long lineNo) const wxOVERRIDE;
    virtual wxString GetLineText(long lineNo) const wxOVERRIDE;
    virtual int GetNumberOfLines() const wxOVERRIDE;
    virtual bool IsModified() const wxOVERRIDE { return m_olModified; }
    virtual void MarkDirty() wxOVERRIDE { m_olModified = true; }
    virtual void DiscardEdits() wxOVERRIDE { m_olModified = false; }
    virtual bool SetStyle(long start, long end, const wxTextAttr& style) wxOVERRIDE;
    virtual bool GetStyle(long position, wxTextAttr& style) wxOVERRIDE;
    virtual bool SetDefaultStyle(const wxTextAttr& style) wxOVERRIDE;
    virtual long XYToPosition(long x, long y) const wxOVERRIDE;
    virtual bool PositionToXY(long pos, long *x, long *y) const wxOVERRIDE;
    virtual void SetMaxLength(unsigned long len) wxOVERRIDE { m_olMaxLength = len; m_olValue = GetValue(); OLRecreate(); }
    virtual void ShowPosition(long pos) wxOVERRIDE;

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

protected:
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual void DoSetValue(const wxString& value, int flags = 0) wxOVERRIDE;

private:
    unsigned long m_olMaxLength;
    bool m_olModified;

    wxDECLARE_DYNAMIC_CLASS(wxTextCtrl);
    wxDECLARE_NO_COPY_CLASS(wxTextCtrl);
};

#endif // _WX_OPENLAYOUT_TEXTCTRL_H_
