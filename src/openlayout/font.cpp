/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/font.cpp
// Purpose:     wxFont: wx's description of a font, opened as an Amiga
//              TextFont through diskfont.library when it is drawn
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/font.h"
#endif

#include "wx/fontutil.h"

#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>

#include "private.h"

// the fonts opened: once each (name, size, style) for the program's life.
// OpenDiskFont looks in FONTS: each time, and wx makes a font for nearly
// every control (Audacity's Preferences opened 3130 and spent over a
// minute in it)
#include <map>
#include <string>
namespace
{
struct OpenFonts
{
    std::map<std::string, struct TextFont *> fonts;
    ~OpenFonts()
    {
        for ( std::map<std::string, struct TextFont *>::iterator it = fonts.begin(); it != fonts.end(); ++it )
            if ( it->second )
                CloseFont(it->second);
    }
};

struct TextFont *OpenCached(const char *name, UWORD ysize, UBYTE style)
{
    static OpenFonts open;
    char key[160];
    snprintf(key, sizeof key, "%s/%u/%u", name, (unsigned)ysize, (unsigned)style);
    std::map<std::string, struct TextFont *>::iterator it = open.fonts.find(key);
    if ( it != open.fonts.end() )
        return it->second;
    struct TextAttr ta;
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = ysize;
    ta.ta_Style = style;
    ta.ta_Flags = 0;
    struct TextFont *tf = OpenDiskFont(&ta);
    open.fonts[key] = tf;   // a font that is not there is not looked for again
    return tf;
}
}

class wxFontRefData : public wxGDIRefData
{
public:
    wxFontRefData() : m_pixels(0), m_tf(NULL), m_tfFor(0) {}
    wxFontRefData(const wxFontRefData& d)
        : wxGDIRefData(), m_info(d.m_info), m_pixels(d.m_pixels), m_tf(NULL), m_tfFor(0) {}
    virtual ~wxFontRefData() { Close(); }

    // the font stays open in the cache
    void Close()
    {
        m_tf = NULL;
        m_tfFor = 0;
    }

    int Pixels() const
    {
        if ( m_pixels > 0 )
            return m_pixels;
        // the port's 72 pixels an inch: a point is a pixel
        int px = (int)(m_info.GetFractionalPointSize() + 0.5);
        return px > 0 ? px : 8;
    }

    wxNativeFontInfo m_info;
    int m_pixels;               // asked for in pixels, else 0
    struct TextFont *m_tf;      // opened at m_tfFor pixels
    int m_tfFor;
};

#define M_FONTDATA ((wxFontRefData *)m_refData)

// (wxFont's class info is in wx's common code)

wxFont::wxFont(const wxFontInfo& info)
{
    m_refData = new wxFontRefData;
    wxNativeFontInfo& n = M_FONTDATA->m_info;
    n.SetFamily(info.GetFamily());
    n.SetStyle(info.GetStyle());
    n.SetNumericWeight(info.GetNumericWeight());
    n.SetUnderlined(info.IsUnderlined());
    n.SetStrikethrough(info.IsStrikethrough());
    if ( info.HasFaceName() )
        n.SetFaceName(info.GetFaceName());
    n.SetEncoding(info.GetEncoding());
    if ( info.IsUsingSizeInPixels() )
    {
        M_FONTDATA->m_pixels = info.GetPixelSize().y;
        n.SetFractionalPointSize(info.GetPixelSize().y);
    }
    else
    {
        n.SetFractionalPointSize(info.GetFractionalPointSize() > 0 ? info.GetFractionalPointSize()
                                                                  : wxNORMAL_FONT->GetFractionalPointSize());
    }
}

wxFont::wxFont(const wxNativeFontInfo& info)
{
    m_refData = new wxFontRefData;
    M_FONTDATA->m_info = info;
}

wxFont::wxFont(const wxString& nativeInfoString)
{
    wxNativeFontInfo info;
    if ( info.FromString(nativeInfoString) )
    {
        m_refData = new wxFontRefData;
        M_FONTDATA->m_info = info;
    }
}

bool wxFont::Create(int size, wxFontFamily family, wxFontStyle style, wxFontWeight weight,
                    bool underlined, const wxString& face, wxFontEncoding encoding)
{
    UnRef();
    m_refData = new wxFontRefData;
    wxNativeFontInfo& n = M_FONTDATA->m_info;
    n.SetFractionalPointSize(size > 0 ? size : wxNORMAL_FONT->GetFractionalPointSize());
    n.SetFamily(family == wxFONTFAMILY_DEFAULT ? wxFONTFAMILY_SWISS : family);
    n.SetStyle(style);
    n.SetWeight(weight);
    n.SetUnderlined(underlined);
    if ( !face.empty() )
        n.SetFaceName(face);
    n.SetEncoding(encoding);
    return true;
}

double wxFont::GetFractionalPointSize() const
{
    wxCHECK_MSG(IsOk(), 0, "invalid font");
    return M_FONTDATA->m_info.GetFractionalPointSize();
}

wxSize wxFont::GetPixelSize() const
{
    wxCHECK_MSG(IsOk(), wxDefaultSize, "invalid font");
    return wxSize(0, M_FONTDATA->Pixels());
}

wxFontStyle wxFont::GetStyle() const { wxCHECK_MSG(IsOk(), wxFONTSTYLE_MAX, "invalid font"); return M_FONTDATA->m_info.GetStyle(); }
int wxFont::GetNumericWeight() const { wxCHECK_MSG(IsOk(), wxFONTWEIGHT_MAX, "invalid font"); return M_FONTDATA->m_info.GetNumericWeight(); }
bool wxFont::GetUnderlined() const { wxCHECK_MSG(IsOk(), false, "invalid font"); return M_FONTDATA->m_info.GetUnderlined(); }
bool wxFont::GetStrikethrough() const { wxCHECK_MSG(IsOk(), false, "invalid font"); return M_FONTDATA->m_info.GetStrikethrough(); }
wxString wxFont::GetFaceName() const { wxCHECK_MSG(IsOk(), wxString(), "invalid font"); return M_FONTDATA->m_info.GetFaceName(); }
wxFontEncoding wxFont::GetEncoding() const { wxCHECK_MSG(IsOk(), wxFONTENCODING_MAX, "invalid font"); return M_FONTDATA->m_info.GetEncoding(); }
const wxNativeFontInfo *wxFont::GetNativeFontInfo() const { return IsOk() ? &M_FONTDATA->m_info : NULL; }
wxFontFamily wxFont::DoGetFamily() const { return M_FONTDATA->m_info.GetFamily(); }

bool wxFont::IsFixedWidth() const
{
    wxCHECK_MSG(IsOk(), false, "invalid font");
    struct TextFont *tf = GetTextFont();
    if ( tf )
        return !(tf->tf_Flags & FPF_PROPORTIONAL);
    return M_FONTDATA->m_info.GetFamily() == wxFONTFAMILY_TELETYPE;
}

void wxFont::SetFractionalPointSize(double pointSize)
{
    AllocExclusive();
    M_FONTDATA->Close();
    M_FONTDATA->m_pixels = 0;
    M_FONTDATA->m_info.SetFractionalPointSize(pointSize);
}

void wxFont::SetPixelSize(const wxSize& pixelSize)
{
    AllocExclusive();
    M_FONTDATA->Close();
    M_FONTDATA->m_pixels = pixelSize.y > 0 ? pixelSize.y : 0;
    if ( pixelSize.y > 0 )
        M_FONTDATA->m_info.SetFractionalPointSize(pixelSize.y);
}

void wxFont::SetFamily(wxFontFamily family) { AllocExclusive(); M_FONTDATA->Close(); M_FONTDATA->m_info.SetFamily(family); }
void wxFont::SetStyle(wxFontStyle style) { AllocExclusive(); M_FONTDATA->Close(); M_FONTDATA->m_info.SetStyle(style); }
void wxFont::SetNumericWeight(int weight) { AllocExclusive(); M_FONTDATA->Close(); M_FONTDATA->m_info.SetNumericWeight(weight); }
bool wxFont::SetFaceName(const wxString& faceName)
{
    AllocExclusive();
    M_FONTDATA->Close();
    M_FONTDATA->m_info.SetFaceName(faceName);
    return wxFontBase::SetFaceName(faceName);
}
void wxFont::SetUnderlined(bool underlined) { AllocExclusive(); M_FONTDATA->m_info.SetUnderlined(underlined); }
void wxFont::SetStrikethrough(bool strikethrough) { AllocExclusive(); M_FONTDATA->m_info.SetStrikethrough(strikethrough); }
void wxFont::SetEncoding(wxFontEncoding encoding) { AllocExclusive(); M_FONTDATA->m_info.SetEncoding(encoding); }

int wxFont::GetPixelHeight() const
{
    return IsOk() ? M_FONTDATA->Pixels() : 8;
}

struct TextFont *wxFont::GetTextFont() const
{
    if ( !IsOk() )
        return NULL;
    wxFontRefData *d = M_FONTDATA;
    int px = d->Pixels();
    if ( d->m_tf && d->m_tfFor == px )
        return d->m_tf;
    d->Close();

    // the face asked for; else the screen's font, or topaz for a fixed one
    wxString face = d->m_info.GetFaceName();
    struct TextFont *screen = wxOLScreenFont();
    std::string name;
    if ( !face.empty() )
    {
        name = wxOLToLatin1(face);
        if ( name.find(".font") == std::string::npos )
            name += ".font";
    }
    else if ( d->m_info.GetFamily() == wxFONTFAMILY_TELETYPE || d->m_info.GetFamily() == wxFONTFAMILY_MODERN )
        name = "topaz.font";
    else if ( screen && screen->tf_Message.mn_Node.ln_Name )
        name = screen->tf_Message.mn_Node.ln_Name;
    else
        name = "topaz.font";

    UBYTE style = 0;
    if ( d->m_info.GetNumericWeight() >= wxFONTWEIGHT_BOLD )
        style |= FSF_BOLD;
    if ( d->m_info.GetStyle() != wxFONTSTYLE_NORMAL )
        style |= FSF_ITALIC;
    d->m_tf = OpenCached(name.c_str(), (UWORD)px, style);
    if ( !d->m_tf && name != "topaz.font" )
        d->m_tf = OpenCached("topaz.font", (UWORD)px, style);
    if ( d->m_tf )
        d->m_tfFor = px;
    return d->m_tf;
}

wxGDIRefData *wxFont::CreateGDIRefData() const { return new wxFontRefData; }
wxGDIRefData *wxFont::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxFontRefData(*(const wxFontRefData *)data);
}
