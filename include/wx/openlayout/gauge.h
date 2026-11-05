/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/gauge.h
// Purpose:     wxGauge: an olui progress bar
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_GAUGE_H_
#define _WX_OPENLAYOUT_GAUGE_H_

class WXDLLIMPEXP_CORE wxGauge : public wxGaugeBase
{
public:
    wxGauge() : m_olRange(100), m_olValue(0){}
    wxGauge(wxWindow *parent, wxWindowID id, int range, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxGA_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxGaugeNameStr)) : m_olRange(100), m_olValue(0)
    {
        Create(parent, id, range, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, int range, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxGA_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxGaugeNameStr));

    virtual void SetRange(int range) wxOVERRIDE;
    virtual int GetRange() const wxOVERRIDE { return m_olRange; }
    virtual void SetValue(int pos) wxOVERRIDE;
    virtual int GetValue() const wxOVERRIDE { return m_olValue; }
    virtual void OLElementMade() wxOVERRIDE;
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }
private:
    int m_olRange, m_olValue;
public:

    wxDECLARE_DYNAMIC_CLASS(wxGauge);
    wxDECLARE_NO_COPY_CLASS(wxGauge);
};

#endif // _WX_OPENLAYOUT_GAUGE_H_
