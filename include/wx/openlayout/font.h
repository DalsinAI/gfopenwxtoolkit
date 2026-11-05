/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/font.h
// Purpose:     wxFont: diskfont.library fonts (GFOpenFont)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_FONT_H_
#define _WX_OPENLAYOUT_FONT_H_

struct TextFont;

// A font is what wx asks for (size in points, family, style, weight, face
// name, underline); the Amiga TextFont it becomes is opened through
// diskfont.library at the pixel size the first time it is drawn, and kept.
class WXDLLIMPEXP_CORE wxFont : public wxFontBase
{
public:
    wxFont() {}
    wxFont(const wxFontInfo& info);
    wxFont(const wxNativeFontInfo& info);
    wxFont(const wxString& nativeInfoString);
    wxFont(int size, wxFontFamily family, wxFontStyle style, wxFontWeight weight,
           bool underlined = false, const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(size, family, style, weight, underlined, face, encoding);
    }
    wxFont(const wxSize& pixelSize, wxFontFamily family, wxFontStyle style, wxFontWeight weight,
           bool underlined = false, const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(10, family, style, weight, underlined, face, encoding);
        SetPixelSize(pixelSize);
    }

    bool Create(int size, wxFontFamily family, wxFontStyle style, wxFontWeight weight,
                bool underlined = false, const wxString& face = wxEmptyString,
                wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    virtual double GetFractionalPointSize() const wxOVERRIDE;
    virtual wxSize GetPixelSize() const wxOVERRIDE;
    virtual wxFontStyle GetStyle() const wxOVERRIDE;
    virtual int GetNumericWeight() const wxOVERRIDE;
    virtual bool GetUnderlined() const wxOVERRIDE;
    virtual bool GetStrikethrough() const wxOVERRIDE;
    virtual wxString GetFaceName() const wxOVERRIDE;
    virtual wxFontEncoding GetEncoding() const wxOVERRIDE;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const wxOVERRIDE;
    virtual bool IsFixedWidth() const wxOVERRIDE;

    virtual void SetFractionalPointSize(double pointSize) wxOVERRIDE;
    virtual void SetPixelSize(const wxSize& pixelSize) wxOVERRIDE;
    virtual void SetFamily(wxFontFamily family) wxOVERRIDE;
    virtual void SetStyle(wxFontStyle style) wxOVERRIDE;
    virtual void SetNumericWeight(int weight) wxOVERRIDE;
    virtual bool SetFaceName(const wxString& faceName) wxOVERRIDE;
    virtual void SetUnderlined(bool underlined) wxOVERRIDE;
    virtual void SetStrikethrough(bool strikethrough) wxOVERRIDE;
    virtual void SetEncoding(wxFontEncoding encoding) wxOVERRIDE;

    wxDECLARE_COMMON_FONT_METHODS();

    // the Amiga font for it, opened at its pixel size (NULL: none could be;
    // the DC then uses the screen's)
    struct TextFont *GetTextFont() const;
    // the pixel height it is drawn at
    int GetPixelHeight() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const wxOVERRIDE;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const wxOVERRIDE;
    virtual wxFontFamily DoGetFamily() const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxFont);
};

#endif // _WX_OPENLAYOUT_FONT_H_
