/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/slider.h
// Purpose:     wxSlider: an olui slider
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_SLIDER_H_
#define _WX_OPENLAYOUT_SLIDER_H_

class WXDLLIMPEXP_CORE wxSlider : public wxSliderBase
{
public:
    wxSlider() : m_olValue(0), m_olMin(0), m_olMax(100), m_olLine(1), m_olPage(10), m_olTick(0), m_olThumb(0){}
    wxSlider(wxWindow *parent, wxWindowID id, int value, int minValue, int maxValue, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSL_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxSliderNameStr)) : m_olValue(0), m_olMin(0), m_olMax(100), m_olLine(1), m_olPage(10), m_olTick(0), m_olThumb(0)
    {
        Create(parent, id, value, minValue, maxValue, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id, int value, int minValue, int maxValue, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxSL_HORIZONTAL, const wxValidator& validator = wxDefaultValidator, const wxString& name = wxASCII_STR(wxSliderNameStr));

    virtual int GetValue() const wxOVERRIDE { return m_olValue; }
    virtual void SetValue(int value) wxOVERRIDE;
    virtual void SetRange(int minValue, int maxValue) wxOVERRIDE;
    virtual int GetMin() const wxOVERRIDE { return m_olMin; }
    virtual int GetMax() const wxOVERRIDE { return m_olMax; }
    virtual int GetTickFreq() const wxOVERRIDE { return m_olTick; }
    virtual void SetLineSize(int lineSize) wxOVERRIDE { m_olLine = lineSize; }
    virtual void SetPageSize(int pageSize) wxOVERRIDE { m_olPage = pageSize; }
    virtual int GetLineSize() const wxOVERRIDE { return m_olLine; }
    virtual int GetPageSize() const wxOVERRIDE { return m_olPage; }
    virtual void SetThumbLength(int len) wxOVERRIDE { m_olThumb = len; }
    virtual int GetThumbLength() const wxOVERRIDE { return m_olThumb; }

    // GFOpenLayout's widget
    virtual wxString OLGetMarkup() const wxOVERRIDE;
    virtual void OLHandleEvent(const struct olui_event& ev) wxOVERRIDE;
    virtual bool OLIsNative() const wxOVERRIDE { return true; }

protected:
    virtual void DoSetTickFreq(int freq) wxOVERRIDE { m_olTick = freq; }
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
private:
    int m_olValue, m_olMin, m_olMax, m_olLine, m_olPage, m_olTick, m_olThumb;
public:

    wxDECLARE_DYNAMIC_CLASS(wxSlider);
    wxDECLARE_NO_COPY_CLASS(wxSlider);
};

#endif // _WX_OPENLAYOUT_SLIDER_H_
