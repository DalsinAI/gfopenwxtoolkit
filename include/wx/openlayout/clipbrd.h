/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/clipbrd.h
// Purpose:     wxClipboard: clipboard.device, unit 0, as IFF FTXT
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_CLIPBRD_H_
#define _WX_OPENLAYOUT_CLIPBRD_H_

class WXDLLIMPEXP_CORE wxClipboard : public wxClipboardBase
{
public:
    wxClipboard();
    virtual ~wxClipboard();

    virtual bool Open() wxOVERRIDE;
    virtual void Close() wxOVERRIDE;
    virtual bool IsOpened() const wxOVERRIDE { return m_open; }
    virtual bool SetData(wxDataObject *data) wxOVERRIDE;
    virtual bool AddData(wxDataObject *data) wxOVERRIDE;
    virtual bool GetData(wxDataObject& data) wxOVERRIDE;
    virtual void Clear() wxOVERRIDE;
    virtual bool IsSupported(const wxDataFormat& format) wxOVERRIDE;

private:
    wxDataObject *m_olData;     // what was set, in this program
    bool m_open;

    wxDECLARE_DYNAMIC_CLASS(wxClipboard);
};

#endif // _WX_OPENLAYOUT_CLIPBRD_H_
