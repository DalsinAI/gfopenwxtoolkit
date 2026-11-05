/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dataobj2.h
// Purpose:     bitmap and file data objects
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DATAOBJ2_H_
#define _WX_OPENLAYOUT_DATAOBJ2_H_

class WXDLLIMPEXP_CORE wxBitmapDataObject : public wxBitmapDataObjectBase
{
public:
    wxBitmapDataObject() {}
    wxBitmapDataObject(const wxBitmap& bitmap) : wxBitmapDataObjectBase(bitmap) {}

    // the pixels go as a PNG
    virtual size_t GetDataSize() const wxOVERRIDE;
    virtual bool GetDataHere(void *buf) const wxOVERRIDE;
    virtual bool SetData(size_t len, const void *buf) wxOVERRIDE;

    virtual size_t GetDataSize(const wxDataFormat&) const wxOVERRIDE { return GetDataSize(); }
    virtual bool GetDataHere(const wxDataFormat&, void *buf) const wxOVERRIDE { return GetDataHere(buf); }
    virtual bool SetData(const wxDataFormat&, size_t len, const void *buf) wxOVERRIDE { return SetData(len, buf); }

private:
    mutable wxMemoryBuffer m_png;
};

class WXDLLIMPEXP_CORE wxFileDataObject : public wxFileDataObjectBase
{
public:
    void AddFile(const wxString& filename) { m_filenames.Add(filename); }

    // the names, one a line
    virtual size_t GetDataSize() const wxOVERRIDE;
    virtual bool GetDataHere(void *buf) const wxOVERRIDE;
    virtual bool SetData(size_t len, const void *buf) wxOVERRIDE;

    virtual size_t GetDataSize(const wxDataFormat&) const wxOVERRIDE { return GetDataSize(); }
    virtual bool GetDataHere(const wxDataFormat&, void *buf) const wxOVERRIDE { return GetDataHere(buf); }
    virtual bool SetData(const wxDataFormat&, size_t len, const void *buf) wxOVERRIDE { return SetData(len, buf); }
};

#endif // _WX_OPENLAYOUT_DATAOBJ2_H_
