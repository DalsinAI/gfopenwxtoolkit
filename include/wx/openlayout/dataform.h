/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dataform.h
// Purpose:     wxDataFormat: an IFF form type, as clipboard.device holds
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DATAFORM_H_
#define _WX_OPENLAYOUT_DATAFORM_H_

// A data format is the IFF form the Amiga clipboard keeps it as (FTXT for
// text, ILBM for pictures), or a name of the program's own.
class WXDLLIMPEXP_CORE wxDataFormat
{
public:
    typedef unsigned long NativeFormat;

    wxDataFormat() : m_type(wxDF_INVALID), m_format(0) {}
    wxDataFormat(wxDataFormatId type) { SetType(type); }
    wxDataFormat(const wxString& id) { SetId(id); }
    wxDataFormat(const char *id) { SetId(wxString(id)); }
    wxDataFormat(NativeFormat format) { SetId(format); }

    wxDataFormat& operator=(NativeFormat format) { SetId(format); return *this; }
    bool operator==(NativeFormat format) const { return m_format == format; }
    bool operator!=(NativeFormat format) const { return m_format != format; }
    bool operator==(wxDataFormatId type) const { return m_type == type; }
    bool operator!=(wxDataFormatId type) const { return m_type != type; }
    bool operator==(const wxDataFormat& other) const
        { return m_type == other.m_type && m_format == other.m_format && m_id == other.m_id; }
    bool operator!=(const wxDataFormat& other) const { return !(*this == other); }

    NativeFormat GetFormatId() const { return m_format; }
    operator NativeFormat() const { return m_format; }
    void SetId(NativeFormat format);

    wxString GetId() const { return m_id; }
    void SetId(const wxString& id);

    wxDataFormatId GetType() const { return m_type; }
    void SetType(wxDataFormatId type);

private:
    wxDataFormatId m_type;
    NativeFormat m_format;
    wxString m_id;
};

#endif // _WX_OPENLAYOUT_DATAFORM_H_
