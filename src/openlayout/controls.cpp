/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/controls.cpp
// Purpose:     wxControl and the simple controls: olui's buttons,
//              checkboxes, radio buttons, labels, boxes, lines, progress
//              bars and sliders; a static bitmap, scroll bar and spin
//              button wx draws itself
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/control.h"
    #include "wx/button.h"
    #include "wx/bmpbuttn.h"
    #include "wx/checkbox.h"
    #include "wx/radiobut.h"
    #include "wx/stattext.h"
    #include "wx/statbox.h"
    #include "wx/statline.h"
    #include "wx/statbmp.h"
    #include "wx/gauge.h"
    #include "wx/slider.h"
    #include "wx/scrolbar.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/toplevel.h"
    #include "wx/artprov.h"
    #include "wx/dcscreen.h"
#endif

#include "wx/tglbtn.h"
#include "wx/spinbutt.h"
#include "wx/tooltip.h"
#include "wx/stockitem.h"

#include "private.h"

// the text a control shows, as olui's markup has it
static wxString Markup(const wxString& label)
{
    wxString s = wxOLEscape(label);
    s.Replace("\n", "<br>");
    return s;
}

static wxString IdAttr(const wxWindow *w)
{
    wxString a = " id=" + w->OLId();
    if ( w->GetToolTip() && !w->GetToolTip()->GetTip().empty() )
        a += " title=\"" + wxOLEscape(w->GetToolTip()->GetTip()) + "\"";
    if ( !w->IsThisEnabled() )
        a += " disabled";
    return a;
}

// ---- wxControl ------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxControl, wxWindow);

bool wxControl::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                       long style, const wxValidator& validator, const wxString& name)
{
    if ( !wxWindow::Create(parent, id, pos, size, style, name) )
        return false;
#if wxUSE_VALIDATORS
    SetValidator(validator);
#else
    (void)validator;
#endif
    return true;
}

wxSize wxControl::DoGetBestSize() const
{
    return wxSize(60, GetCharHeight() + 8);
}

wxVisualAttributes wxControl::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    wxVisualAttributes a;
    a.font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    a.colFg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
    a.colBg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    return a;
}

// the size a control's text takes, and a native widget's room round it
static wxSize TextBox(const wxWindow *w, const wxString& text, int padx, int pady)
{
    int tw = 0, th = 0;
    wxArrayString lines = wxSplit(text, '\n', '\0');
    if ( lines.empty() )
        lines.push_back(wxString());
    for ( size_t i = 0; i < lines.size(); i++ )
    {
        int lw, lh;
        w->GetTextExtent(lines[i].empty() ? wxString("W") : lines[i], &lw, &lh);
        if ( lines[i].empty() )
            lw = 0;
        tw = wxMax(tw, lw);
        th += lh;
    }
    return wxSize(tw + padx, th + pady);
}

// ---- buttons -------------------------------------------------------------------------------

void wxAnyButton::SetLabel(const wxString& label)
{
    wxAnyButtonBase::SetLabel(label);
    OLRecreate();
}

wxString wxAnyButton::OLGetMarkup() const
{
    wxString inner;
    if ( m_olBitmaps[State_Normal].IsOk() )
    {
        const wxSize s = m_olBitmaps[State_Normal].GetDefaultSize();
        inner = wxString::Format("<img image=%d width=%d height=%d>", m_olImage, s.x, s.y);
    }
    if ( !GetLabelText().empty() && !(GetWindowStyleFlag() & wxBU_NOTEXT) )
        inner += Markup(GetLabelText());
    return "<button" + IdAttr(this) + ">" + inner + "</button>";
}

void wxAnyButton::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CLICK )
        return;
    wxCommandEvent e(wxEVT_BUTTON, GetId());
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

wxSize wxAnyButton::DoGetBestSize() const
{
    wxSize s(0, 0);
    if ( !GetLabelText().empty() && !(GetWindowStyleFlag() & wxBU_NOTEXT) )
        s = TextBox(this, GetLabelText(), 0, 0);
    if ( m_olBitmaps[State_Normal].IsOk() )
    {
        const wxSize b = m_olBitmaps[State_Normal].GetDefaultSize();
        s.x += b.x + (s.x ? 4 : 0);
        s.y = wxMax(s.y, b.y);
    }
    s.x += 16;
    s.y += 8;
    if ( !(GetWindowStyleFlag() & wxBU_EXACTFIT) && !GetLabelText().empty() )
        s.x = wxMax(s.x, 60);
    return s;
}

wxBitmap wxAnyButton::DoGetBitmap(State state) const
{
    return m_olBitmaps[state].GetBitmap(wxDefaultSize);
}

void wxAnyButton::DoSetBitmap(const wxBitmapBundle& bitmap, State which)
{
    const bool had = m_olBitmaps[State_Normal].IsOk();
    m_olBitmaps[which] = bitmap;
    if ( which == State_Normal )
    {
        if ( !had )
            Bind(wxEVT_PAINT, &wxAnyButton::OLOnPaint, this);
        OLRecreate();
        InvalidateBestSize();
    }
    else
        Refresh();
}

void wxAnyButton::OLOnPaint(wxPaintEvent& WXUNUSED(event))
{
    // the img in the button: the state's bitmap, else the normal one
    wxPaintDC dc(this);
    State st = !IsEnabled() ? State_Disabled : State_Normal;
    wxBitmap bmp = m_olBitmaps[st].IsOk() ? m_olBitmaps[st].GetBitmap(wxDefaultSize)
                                          : m_olBitmaps[State_Normal].GetBitmap(wxDefaultSize);
    if ( bmp.IsOk() )
        dc.DrawBitmap(bmp, 0, 0, true);
}

// (wxButton's class info is in wx's common code)

wxSize wxButtonBase::GetDefaultSize(wxWindow *win)
{
    // the size DoGetBestSize gives a short text button: a line of the
    // window's (or the GUI) font with 8 pixels round it, and 60 wide
    int h;
    if ( win )
        h = win->GetCharHeight();
    else
    {
        wxScreenDC dc;
        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        h = dc.GetCharHeight();
    }
    return wxSize(60, h + 8);
}

bool wxButton::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                      const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_labelOrig = label.empty() && wxIsStockID(id) ? wxGetStockLabel(id) : label;
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxWindow *wxButton::SetDefault()
{
    wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
    wxCHECK_MSG(tlw, NULL, "button without top level window?");
    return tlw->SetDefaultItem(this);
}

// (wxBitmapButton's class info is in wx's common code)

bool wxBitmapButton::Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& bitmap, const wxPoint& pos,
                            const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_olBitmaps[State_Normal] = bitmap;
    if ( bitmap.IsOk() )
        Bind(wxEVT_PAINT, &wxBitmapButton::OLOnPaint, this);
    if ( !wxButton::Create(parent, id, wxString(), pos, size, style | wxBU_EXACTFIT, validator, name) )
        return false;
    return true;
}

// CreateCloseButton: wx's common bmpbtncmn.cpp (art provider close bitmap)

wxDEFINE_EVENT( wxEVT_TOGGLEBUTTON, wxCommandEvent );

wxIMPLEMENT_DYNAMIC_CLASS(wxToggleButton, wxControl);

bool wxToggleButton::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                            const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_labelOrig = label;
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxToggleButton::OLGetMarkup() const
{
    wxString m = wxAnyButton::OLGetMarkup();
    if ( m_olValue )
        m.Replace("<button", "<button checked", false);
    return m;
}

void wxToggleButton::SetValue(bool state)
{
    m_olValue = state;
    if ( m_olEl && OLGetUI() )
        olui_set_checked(OLGetUI(), m_olEl, state);
}

void wxToggleButton::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CLICK )
        return;
    SetValue(!m_olValue);
    wxCommandEvent e(wxEVT_TOGGLEBUTTON, GetId());
    e.SetInt(m_olValue);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmapToggleButton, wxToggleButton);

bool wxBitmapToggleButton::Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label,
                                  const wxPoint& pos, const wxSize& size, long style,
                                  const wxValidator& validator, const wxString& name)
{
    m_olBitmaps[State_Normal] = label;
    if ( label.IsOk() )
        Bind(wxEVT_PAINT, &wxBitmapToggleButton::OLOnPaint, this);
    return wxToggleButton::Create(parent, id, wxString(), pos, size, style | wxBU_EXACTFIT, validator, name);
}

// ---- checkboxes and radio buttons -------------------------------------------------------------

// (wxCheckBox's class info is in wx's common code)

bool wxCheckBox::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                        const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_labelOrig = label;
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxCheckBox::OLGetMarkup() const
{
    // the text inside the box's element: olui leaves the box room at its
    // start, and a click on the text ticks it (a text beside it, in a
    // label, was laid over the box)
    return "<checkbox" + IdAttr(this) + (m_olState == wxCHK_CHECKED ? " checked" : "") + ">" +
           Markup(GetLabelText()) + "</checkbox>";
}

void wxCheckBox::SetValue(bool value)
{
    DoSet3StateValue(value ? wxCHK_CHECKED : wxCHK_UNCHECKED);
}

void wxCheckBox::DoSet3StateValue(wxCheckBoxState state)
{
    m_olState = state;
    if ( struct olui_el *box = m_olEl ? olui_find(OLGetUI(), OLId().utf8_str()) : NULL )
        olui_set_checked(OLGetUI(), box, state == wxCHK_CHECKED);
}

void wxCheckBox::SetLabel(const wxString& label)
{
    wxCheckBoxBase::SetLabel(label);
    OLRecreate();
}

void wxCheckBox::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CHANGE )
        return;
    m_olState = ev.checked ? wxCHK_CHECKED : wxCHK_UNCHECKED;
    wxCommandEvent e(wxEVT_CHECKBOX, GetId());
    e.SetInt(m_olState);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

// (wxRadioButton's class info is in wx's common code)

bool wxRadioButton::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                           const wxSize& size, long style, const wxValidator& validator, const wxString& name)
{
    m_labelOrig = label;
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    // the first of a group is on
    if ( HasFlag(wxRB_GROUP) || !GetPreviousInGroup() )
        SetValue(true);
    return true;
}

wxString wxRadioButton::OLGetMarkup() const
{
    // a group's buttons share the name of its first
    const wxRadioButton *first = this;
    while ( const wxRadioButton *p = first->GetPreviousInGroup() )
        first = p;
    return "<radio" + IdAttr(this) + " name=g" + first->OLId() + (m_olValue ? " checked" : "") + ">" +
           Markup(GetLabelText()) + "</radio>";
}

void wxRadioButton::SetValue(bool value)
{
    m_olValue = value;
    struct olui *ui = OLGetUI();
    if ( struct olui_el *r = m_olEl && ui ? olui_find(ui, OLId().utf8_str()) : NULL )
    {
        olui_set_checked(ui, r, value);
        // olui reports the radio it turned on: the program's own change is
        // no event in wx, and a stale one would turn it back later (two
        // set radios of a group then trade places for ever)
        if ( wxNonOwnedWindow *tlw = OLGetTLW() )
            tlw->OLDropQueued(this, OLUI_CHANGE);
    }
    if ( !value )
        return;
    // the others of its group go off
    for ( wxRadioButton *p = GetPreviousInGroup(); p; p = p->GetPreviousInGroup() )
        if ( p->m_olValue )
            p->SetValue(false);
    for ( wxRadioButton *n = GetNextInGroup(); n; n = n->GetNextInGroup() )
        if ( n->m_olValue )
            n->SetValue(false);
}

void wxRadioButton::SetLabel(const wxString& label)
{
    wxRadioButtonBase::SetLabel(label);
    OLRecreate();
}

void wxRadioButton::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CHANGE || !ev.checked || m_olValue )
        return;
    SetValue(true);
    wxCommandEvent e(wxEVT_RADIOBUTTON, GetId());
    e.SetInt(1);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

// ---- labels, boxes, lines -------------------------------------------------------------------------

// (wxStaticText's class info is in wx's common code)

bool wxStaticText::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                          const wxSize& size, long style, const wxString& name)
{
    m_labelOrig = label;
    m_olVisible = GetLabelText(label);
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxStaticText::OLGetMarkup() const
{
    wxString align = "left";
    if ( HasFlag(wxALIGN_CENTRE_HORIZONTAL) )
        align = "center";
    else if ( HasFlag(wxALIGN_RIGHT) )
        align = "right";
    return "<div" + IdAttr(this) + " style='text-align:" + align + "'>" + Markup(m_olVisible) + "</div>";
}

void wxStaticText::SetLabel(const wxString& label)
{
    m_labelOrig = label;
    WXSetVisibleLabel(GetLabelText(label));
}

void wxStaticText::WXSetVisibleLabel(const wxString& str)
{
    if ( str == m_olVisible )
        return;
    m_olVisible = str;
    struct olui *ui = OLGetUI();
    if ( m_olEl && ui && str.Find('\n') == wxNOT_FOUND )
        olui_set_text(ui, m_olEl, wxOLToUTF8(str).c_str());
    else
        OLRecreate();
    InvalidateBestSize();
    if ( !HasFlag(wxST_NO_AUTORESIZE) )
        SetSize(GetBestSize());
}

wxSize wxStaticText::DoGetBestSize() const
{
    return TextBox(this, m_olVisible, 2, 2);
}

// (wxStaticBox's class info is in wx's common code)

bool wxStaticBox::Create(wxWindow *parent, wxWindowID id, const wxString& label, const wxPoint& pos,
                         const wxSize& size, long style, const wxString& name)
{
    m_labelOrig = label;
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxStaticBox::OLGetMarkup() const
{
    wxString m = "<fieldset" + IdAttr(this) + " style='padding:0'>";
    if ( !GetLabelText().empty() )
        m += "<legend>" + Markup(GetLabelText()) + "</legend>";
    return m + "</fieldset>";
}

void wxStaticBox::GetBordersForSizer(int *borderTop, int *borderOther) const
{
    *borderTop = GetLabelText().empty() ? 6 : GetCharHeight() + 4;
    *borderOther = 6;
}

void wxStaticBox::SetLabel(const wxString& label)
{
    wxStaticBoxBase::SetLabel(label);
    OLRecreate();
}

// (wxStaticLine's class info is in wx's common code)

bool wxStaticLine::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                          long style, const wxString& name)
{
    wxSize s = AdjustSize(size);
    if ( !wxControl::Create(parent, id, pos, s, style, wxDefaultValidator, name) )
        return false;
    SetInitialSize(s);
    return true;
}

wxString wxStaticLine::OLGetMarkup() const
{
    if ( IsVertical() )
        return "<div" + IdAttr(this) + " style='border-left:1px solid #808080;border-right:1px solid #ffffff'></div>";
    return "<hr" + IdAttr(this) + ">";
}

// ---- a static bitmap: wx draws it ---------------------------------------------------------------

// (wxStaticBitmap's class info is in wx's common code)

bool wxStaticBitmap::Create(wxWindow *parent, wxWindowID id, const wxBitmapBundle& label, const wxPoint& pos,
                            const wxSize& size, long style, const wxString& name)
{
    m_bitmapBundle = label;
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    Bind(wxEVT_PAINT, &wxStaticBitmap::OnPaint, this);
    SetInitialSize(size);
    return true;
}

void wxStaticBitmap::SetBitmap(const wxBitmapBundle& bitmap)
{
    m_bitmapBundle = bitmap;
    InvalidateBestSize();
    SetSize(GetBestSize());
    Refresh();
}

wxBitmap wxStaticBitmap::GetBitmap() const
{
    return m_bitmapBundle.GetBitmap(wxDefaultSize);
}

void wxStaticBitmap::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    wxBitmap bmp = GetBitmap();
    if ( !bmp.IsOk() )
        return;
    const wxSize s = GetClientSize();
    dc.DrawBitmap(bmp, (s.x - bmp.GetWidth()) / 2, (s.y - bmp.GetHeight()) / 2, true);
}

// ---- progress and sliders ----------------------------------------------------------------------------

// (wxGauge's class info is in wx's common code)

bool wxGauge::Create(wxWindow *parent, wxWindowID id, int range, const wxPoint& pos, const wxSize& size,
                     long style, const wxValidator& validator, const wxString& name)
{
    m_olRange = range > 0 ? range : 100;
    m_olValue = 0;
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size.IsFullySpecified() ? size : wxSize(size.x > 0 ? size.x : 100, size.y > 0 ? size.y : 16));
    return true;
}

wxString wxGauge::OLGetMarkup() const
{
    return wxString::Format("<progress%s max=%d></progress>", IdAttr(this), m_olRange);
}

void wxGauge::OLElementMade()
{
    olui_set_progress(OLGetUI(), m_olEl, m_olValue);
}

void wxGauge::SetRange(int range)
{
    if ( range <= 0 || range == m_olRange )
        return;
    m_olRange = range;
    m_olValue = wxMin(m_olValue, m_olRange);
    OLRecreate();
}

void wxGauge::SetValue(int pos)
{
    m_olValue = wxMax(0, wxMin(pos, m_olRange));
    if ( m_olEl && OLGetUI() )
        olui_set_progress(OLGetUI(), m_olEl, m_olValue);
}

// (wxSlider's class info is in wx's common code)

bool wxSlider::Create(wxWindow *parent, wxWindowID id, int value, int minValue, int maxValue,
                      const wxPoint& pos, const wxSize& size, long style, const wxValidator& validator,
                      const wxString& name)
{
    m_olMin = minValue;
    m_olMax = maxValue > minValue ? maxValue : minValue + 1;
    m_olValue = wxMax(m_olMin, wxMin(value, m_olMax));
    m_olPage = wxMax(1, (m_olMax - m_olMin) / 10);
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetInitialSize(size);
    return true;
}

wxString wxSlider::OLGetMarkup() const
{
    // a slider whose end is its minimum (wxSL_INVERSE): olui has it from
    // min to max, wx the other way
    return wxString::Format("<slider%s min=%d max=%d value=%d>", IdAttr(this), m_olMin, m_olMax,
                            HasFlag(wxSL_INVERSE) ? m_olMax + m_olMin - m_olValue : m_olValue);
}

void wxSlider::SetValue(int value)
{
    m_olValue = wxMax(m_olMin, wxMin(value, m_olMax));
    if ( m_olEl && OLGetUI() )
        olui_set_slider(OLGetUI(), m_olEl, HasFlag(wxSL_INVERSE) ? m_olMax + m_olMin - m_olValue : m_olValue);
}

void wxSlider::SetRange(int minValue, int maxValue)
{
    m_olMin = minValue;
    m_olMax = maxValue > minValue ? maxValue : minValue + 1;
    m_olValue = wxMax(m_olMin, wxMin(m_olValue, m_olMax));
    OLRecreate();
}

void wxSlider::OLHandleEvent(const struct olui_event& ev)
{
    if ( ev.type != OLUI_CHANGE )
        return;
    const int v = HasFlag(wxSL_INVERSE) ? m_olMax + m_olMin - ev.index : ev.index;
    if ( v == m_olValue )
        return;
    m_olValue = v;
    wxScrollEvent se(wxEVT_SCROLL_THUMBTRACK, GetId(), v, HasFlag(wxSL_VERTICAL) ? wxVERTICAL : wxHORIZONTAL);
    se.SetEventObject(this);
    HandleWindowEvent(se);
    wxCommandEvent ce(wxEVT_SLIDER, GetId());
    ce.SetInt(v);
    ce.SetEventObject(this);
    HandleWindowEvent(ce);
}

wxSize wxSlider::DoGetBestSize() const
{
    const int h = GetCharHeight() + 8;
    return HasFlag(wxSL_VERTICAL) ? wxSize(h, 100) : wxSize(100, h);
}

// ---- drawing what olui has no widget for ------------------------------------------------------------

namespace
{
// a raised (or, pressed, sunk) box in the system's colours
void Bevel(wxDC& dc, const wxRect& r, bool pressed)
{
    const wxColour face = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    const wxColour light = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT);
    const wxColour dark = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(face));
    dc.DrawRectangle(r);
    dc.SetPen(wxPen(pressed ? dark : light));
    dc.DrawLine(r.x, r.y, r.GetRight(), r.y);
    dc.DrawLine(r.x, r.y, r.x, r.GetBottom());
    dc.SetPen(wxPen(pressed ? light : dark));
    dc.DrawLine(r.x, r.GetBottom(), r.GetRight() + 1, r.GetBottom());
    dc.DrawLine(r.GetRight(), r.y, r.GetRight(), r.GetBottom());
}

// an arrow in a box: dir 0 up, 1 down, 2 left, 3 right
void Arrow(wxDC& dc, const wxRect& r, int dir, bool enabled)
{
    const int s = wxMax(2, wxMin(r.width, r.height) / 4);
    const int cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    wxPoint p[3];
    switch ( dir )
    {
        case 0: p[0] = wxPoint(cx - s, cy + s / 2); p[1] = wxPoint(cx + s, cy + s / 2); p[2] = wxPoint(cx, cy - s / 2 - 1); break;
        case 1: p[0] = wxPoint(cx - s, cy - s / 2); p[1] = wxPoint(cx + s, cy - s / 2); p[2] = wxPoint(cx, cy + s / 2 + 1); break;
        case 2: p[0] = wxPoint(cx + s / 2, cy - s); p[1] = wxPoint(cx + s / 2, cy + s); p[2] = wxPoint(cx - s / 2 - 1, cy); break;
        default: p[0] = wxPoint(cx - s / 2, cy - s); p[1] = wxPoint(cx - s / 2, cy + s); p[2] = wxPoint(cx + s / 2 + 1, cy); break;
    }
    const wxColour c = wxSystemSettings::GetColour(enabled ? wxSYS_COLOUR_BTNTEXT : wxSYS_COLOUR_GRAYTEXT);
    dc.SetPen(wxPen(c));
    dc.SetBrush(wxBrush(c));
    dc.DrawPolygon(3, p);
}
}

// ---- wxScrollBar ---------------------------------------------------------------------------------------

// (wxScrollBar's class info is in wx's common code)

bool wxScrollBar::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style,
                         const wxValidator& validator, const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &wxScrollBar::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &wxScrollBar::OnMouse, this);
    Bind(wxEVT_LEFT_DCLICK, &wxScrollBar::OnMouse, this);
    Bind(wxEVT_LEFT_UP, &wxScrollBar::OnMouse, this);
    Bind(wxEVT_MOTION, &wxScrollBar::OnMouse, this);
    Bind(wxEVT_MOUSEWHEEL, &wxScrollBar::OnMouse, this);
    m_olRepeat.SetOwner(this);
    Bind(wxEVT_TIMER, &wxScrollBar::OnTimer, this);
    SetInitialSize(size);
    return true;
}

wxSize wxScrollBar::DoGetBestSize() const
{
    return IsVertical() ? wxSize(16, 64) : wxSize(64, 16);
}

void wxScrollBar::OLThumb(int *at, int *len, int *track, int *arrow) const
{
    const wxSize s = GetClientSize();
    const int length = IsVertical() ? s.y : s.x, across = IsVertical() ? s.x : s.y;
    *arrow = wxMin(across, length / 3);
    *track = wxMax(0, length - 2 * *arrow);
    if ( m_olRange <= m_olThumb || m_olRange <= 0 )
    {
        *at = *arrow;
        *len = *track;
        return;
    }
    *len = wxMax(wxMin(8, *track), (int)((long long)*track * m_olThumb / m_olRange));
    const int room = *track - *len;
    *at = *arrow + (int)((long long)room * m_olPos / wxMax(1, m_olRange - m_olThumb));
}

void wxScrollBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    const wxSize s = GetClientSize();
    const bool v = IsVertical();
    int at, len, track, arrow;
    OLThumb(&at, &len, &track, &arrow);
    // the track, sunk
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_SCROLLBAR)));
    dc.DrawRectangle(0, 0, s.x, s.y);
    const bool on = IsEnabled() && m_olRange > m_olThumb;
    if ( v )
    {
        Bevel(dc, wxRect(0, 0, s.x, arrow), m_olHeld == -1);
        Arrow(dc, wxRect(0, 0, s.x, arrow), 0, on);
        Bevel(dc, wxRect(0, s.y - arrow, s.x, arrow), m_olHeld == 1);
        Arrow(dc, wxRect(0, s.y - arrow, s.x, arrow), 1, on);
        if ( on && len > 0 )
            Bevel(dc, wxRect(0, at, s.x, len), false);
    }
    else
    {
        Bevel(dc, wxRect(0, 0, arrow, s.y), m_olHeld == -1);
        Arrow(dc, wxRect(0, 0, arrow, s.y), 2, on);
        Bevel(dc, wxRect(s.x - arrow, 0, arrow, s.y), m_olHeld == 1);
        Arrow(dc, wxRect(s.x - arrow, 0, arrow, s.y), 3, on);
        if ( on && len > 0 )
            Bevel(dc, wxRect(at, 0, len, s.y), false);
    }
}

void wxScrollBar::OLScrollTo(int pos, wxEventType type)
{
    pos = wxMax(0, wxMin(pos, m_olRange - m_olThumb));
    if ( pos == m_olPos && type != wxEVT_SCROLL_THUMBRELEASE && type != wxEVT_SCROLL_CHANGED )
        return;
    m_olPos = pos;
    Refresh();
    wxScrollEvent e(type, GetId(), pos, IsVertical() ? wxVERTICAL : wxHORIZONTAL);
    e.SetEventObject(this);
    HandleWindowEvent(e);
}

void wxScrollBar::OnMouse(wxMouseEvent& event)
{
    if ( !IsEnabled() || m_olRange <= m_olThumb )
        return;
    const int p = IsVertical() ? event.GetY() : event.GetX();
    int at, len, track, arrow;
    OLThumb(&at, &len, &track, &arrow);
    const wxEventType t = event.GetEventType();
    if ( t == wxEVT_MOUSEWHEEL )
    {
        const int lines = event.GetWheelRotation() > 0 ? -event.GetLinesPerAction() : event.GetLinesPerAction();
        OLScrollTo(m_olPos + lines, lines < 0 ? wxEVT_SCROLL_LINEUP : wxEVT_SCROLL_LINEDOWN);
        return;
    }
    if ( t == wxEVT_LEFT_DOWN || t == wxEVT_LEFT_DCLICK )
    {
        if ( !HasCapture() )
            CaptureMouse();
        if ( p < arrow )
            m_olHeld = -1;
        else if ( p >= arrow + track )
            m_olHeld = 1;
        else if ( p >= at && p < at + len )
        {
            m_olDrag = p;
            m_olDragFrom = m_olPos;
            return;
        }
        else
            m_olHeld = p < at ? -2 : 2;
        wxTimerEvent te(m_olRepeat);
        OnTimer(te);
        m_olRepeat.Start(300, wxTIMER_ONE_SHOT);
        return;
    }
    if ( t == wxEVT_MOTION )
    {
        if ( m_olDrag >= 0 && track > len )
        {
            const int moved = p - m_olDrag;
            const int pos = m_olDragFrom + (int)((long long)moved * (m_olRange - m_olThumb) / (track - len));
            OLScrollTo(pos, wxEVT_SCROLL_THUMBTRACK);
        }
        return;
    }
    if ( t == wxEVT_LEFT_UP )
    {
        if ( HasCapture() )
            ReleaseMouse();
        m_olRepeat.Stop();
        if ( m_olDrag >= 0 )
            OLScrollTo(m_olPos, wxEVT_SCROLL_THUMBRELEASE);
        if ( m_olDrag >= 0 || m_olHeld )
            OLScrollTo(m_olPos, wxEVT_SCROLL_CHANGED);
        m_olDrag = -1;
        m_olHeld = 0;
        Refresh();
    }
}

void wxScrollBar::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    switch ( m_olHeld )
    {
        case -1: OLScrollTo(m_olPos - 1, wxEVT_SCROLL_LINEUP); break;
        case 1: OLScrollTo(m_olPos + 1, wxEVT_SCROLL_LINEDOWN); break;
        case -2: OLScrollTo(m_olPos - m_olPage, wxEVT_SCROLL_PAGEUP); break;
        case 2: OLScrollTo(m_olPos + m_olPage, wxEVT_SCROLL_PAGEDOWN); break;
        default: return;
    }
    if ( m_olHeld && !m_olRepeat.IsRunning() )
        m_olRepeat.Start(50, wxTIMER_ONE_SHOT);
}

void wxScrollBar::SetThumbPosition(int viewStart)
{
    viewStart = wxMax(0, wxMin(viewStart, m_olRange - m_olThumb));
    if ( viewStart != m_olPos )
    {
        m_olPos = viewStart;
        Refresh();
    }
}

void wxScrollBar::SetScrollbar(int position, int thumbSize, int range, int pageSize, bool refresh)
{
    m_olRange = wxMax(range, 0);
    m_olThumb = wxMax(thumbSize, 0);
    m_olPage = wxMax(pageSize, 1);
    m_olPos = wxMax(0, wxMin(position, m_olRange - m_olThumb));
    if ( refresh )
        Refresh();
}

// ---- wxSpinButton -------------------------------------------------------------------------------------

// (wxSpinButton's class info is in wx's common code)

bool wxSpinButton::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style,
                          const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &wxSpinButton::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &wxSpinButton::OnMouse, this);
    Bind(wxEVT_LEFT_DCLICK, &wxSpinButton::OnMouse, this);
    Bind(wxEVT_LEFT_UP, &wxSpinButton::OnMouse, this);
    m_olRepeat.SetOwner(this);
    Bind(wxEVT_TIMER, &wxSpinButton::OnTimer, this);
    SetInitialSize(size);
    return true;
}

wxSize wxSpinButton::DoGetBestSize() const
{
    const int h = GetCharHeight() + 8;
    return HasFlag(wxSP_HORIZONTAL) ? wxSize(h * 2, h) : wxSize(16, h);
}

void wxSpinButton::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    const wxSize s = GetClientSize();
    if ( !HasFlag(wxSP_HORIZONTAL) )
    {
        const wxRect up(0, 0, s.x, s.y / 2), down(0, s.y / 2, s.x, s.y - s.y / 2);
        Bevel(dc, up, m_olHeld == 1);
        Arrow(dc, up, 0, IsEnabled());
        Bevel(dc, down, m_olHeld == -1);
        Arrow(dc, down, 1, IsEnabled());
    }
    else
    {
        const wxRect down(0, 0, s.x / 2, s.y), up(s.x / 2, 0, s.x - s.x / 2, s.y);
        Bevel(dc, down, m_olHeld == -1);
        Arrow(dc, down, 2, IsEnabled());
        Bevel(dc, up, m_olHeld == 1);
        Arrow(dc, up, 3, IsEnabled());
    }
}

void wxSpinButton::OLStep(int dir)
{
    int v = m_olValue + dir;
    if ( v > m_max )
        v = HasFlag(wxSP_WRAP) ? m_min : m_max;
    else if ( v < m_min )
        v = HasFlag(wxSP_WRAP) ? m_max : m_min;
    if ( v == m_olValue )
        return;
    wxSpinEvent e(dir > 0 ? wxEVT_SPIN_UP : wxEVT_SPIN_DOWN, GetId());
    e.SetPosition(v);
    e.SetEventObject(this);
    if ( HandleWindowEvent(e) && !e.IsAllowed() )
        return;
    m_olValue = v;
    wxSpinEvent s(wxEVT_SPIN, GetId());
    s.SetPosition(v);
    s.SetEventObject(this);
    HandleWindowEvent(s);
}

void wxSpinButton::OnMouse(wxMouseEvent& event)
{
    if ( !IsEnabled() )
        return;
    const wxSize s = GetClientSize();
    if ( event.GetEventType() == wxEVT_LEFT_UP )
    {
        m_olRepeat.Stop();
        m_olHeld = 0;
        Refresh();
        return;
    }
    m_olHeld = !HasFlag(wxSP_HORIZONTAL) ? (event.GetY() < s.y / 2 ? 1 : -1) : (event.GetX() < s.x / 2 ? -1 : 1);
    Refresh();
    OLStep(m_olHeld);
    m_olRepeat.Start(400, wxTIMER_ONE_SHOT);
}

void wxSpinButton::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    if ( !m_olHeld )
        return;
    OLStep(m_olHeld);
    m_olRepeat.Start(80, wxTIMER_ONE_SHOT);
}

void wxSpinButton::SetValue(int val)
{
    m_olValue = wxMax(m_min, wxMin(val, m_max));
}

void wxSpinButton::SetRange(int min, int max)
{
    wxSpinButtonBase::SetRange(min, max);
    m_olValue = wxMax(m_min, wxMin(m_olValue, m_max));
}
