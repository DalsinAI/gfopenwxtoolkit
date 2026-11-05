/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/textentry.h
// Purpose:     wxTextEntry: the text of an olui input or textarea
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_TEXTENTRY_H_
#define _WX_OPENLAYOUT_TEXTENTRY_H_

class WXDLLIMPEXP_CORE wxTextEntry : public wxTextEntryBase
{
public:
    wxTextEntry() : m_olEditable(true) {}

    virtual void WriteText(const wxString& text) wxOVERRIDE;
    virtual void Remove(long from, long to) wxOVERRIDE;
    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;
    virtual void Undo() wxOVERRIDE {}
    virtual void Redo() wxOVERRIDE {}
    virtual bool CanUndo() const wxOVERRIDE { return false; }
    virtual bool CanRedo() const wxOVERRIDE { return false; }
    virtual void SetInsertionPoint(long pos) wxOVERRIDE;
    virtual long GetInsertionPoint() const wxOVERRIDE;
    virtual long GetLastPosition() const wxOVERRIDE;
    virtual void SetSelection(long from, long to) wxOVERRIDE;
    virtual void GetSelection(long *from, long *to) const wxOVERRIDE;
    virtual bool IsEditable() const wxOVERRIDE { return m_olEditable; }
    virtual void SetEditable(bool editable) wxOVERRIDE;

protected:
    virtual wxString DoGetValue() const wxOVERRIDE;
    virtual void DoSetValue(const wxString& value, int flags = 0) wxOVERRIDE;
    virtual wxWindow *GetEditableWindow() wxOVERRIDE;

    // the olui element holding the text (the window's own element by
    // default; a combo box's input inside it)
    virtual struct olui_el *OLGetTextElement() const;
    // the value while there is no element yet
    wxString m_olValue;
    bool m_olEditable;
};

#endif // _WX_OPENLAYOUT_TEXTENTRY_H_
