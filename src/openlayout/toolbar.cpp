/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/toolbar.cpp
// Purpose:     wxToolBar: tool buttons wx draws, each a child window of
//              its own (its own element, so olui shows each one's
//              tooltip), laid out in a row or a column with the controls
//              added to the bar
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TOOLBAR

#ifndef WX_PRECOMP
    #include "wx/toolbar.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/frame.h"
    #include "wx/image.h"
#endif

#include "wx/tooltip.h"

#include "private.h"

namespace
{
// room round a tool's picture, and a separator's width
const int TOOL_PAD = 3;
const int SEP_SIZE = 8;

// a raised (or, pressed, sunk) box in the system's colours
void ToolBevel(wxDC& dc, const wxRect& r, bool pressed)
{
    const wxColour light = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT);
    const wxColour dark = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
    dc.SetPen(wxPen(pressed ? dark : light));
    dc.DrawLine(r.x, r.y, r.GetRight(), r.y);
    dc.DrawLine(r.x, r.y, r.x, r.GetBottom());
    dc.SetPen(wxPen(pressed ? light : dark));
    dc.DrawLine(r.x, r.GetBottom(), r.GetRight() + 1, r.GetBottom());
    dc.DrawLine(r.GetRight(), r.y, r.GetRight(), r.GetBottom());
}

// one tool's button: drawn flat, raised under the pointer, sunk while
// pressed or toggled on
class wxOLToolButton : public wxWindow
{
public:
    wxOLToolButton(wxToolBar *bar, wxToolBarToolBase *tool)
        : m_bar(bar), m_tool(tool), m_hover(false), m_pressed(false)
    {
        Create(bar, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &wxOLToolButton::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_LEFT_DCLICK, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_LEFT_UP, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_RIGHT_UP, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_MOTION, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_ENTER_WINDOW, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_LEAVE_WINDOW, &wxOLToolButton::OnMouse, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &wxOLToolButton::OnCaptureLost, this);
        UpdateTip();
    }

    virtual bool AcceptsFocus() const wxOVERRIDE { return false; }

    void UpdateTip()
    {
#if wxUSE_TOOLTIPS
        if ( m_tool->GetShortHelp() != (GetToolTip() ? GetToolTip()->GetTip() : wxString()) )
            SetToolTip(m_tool->GetShortHelp());
#endif
    }

    wxSize Wanted() const
    {
        wxSize s = m_bar->GetToolBitmapSize();
        const wxBitmap b = m_tool->GetNormalBitmap();
        if ( b.IsOk() )
            s.IncTo(b.GetSize());
        if ( m_bar->HasFlag(wxTB_NOICONS) )
            s = wxSize(0, 0);
        if ( m_bar->HasFlag(wxTB_TEXT) && !m_tool->GetLabel().empty() )
        {
            int w, h;
            m_bar->GetTextExtent(wxStripMenuCodes(m_tool->GetLabel()), &w, &h);
            if ( m_bar->HasFlag(wxTB_HORZ_LAYOUT) )
                s = wxSize(s.x + (s.x ? TOOL_PAD : 0) + w, wxMax(s.y, h));
            else
                s = wxSize(wxMax(s.x, w), s.y + (s.y ? 1 : 0) + h);
        }
        return s + wxSize(2 * TOOL_PAD, 2 * TOOL_PAD);
    }

private:
    void OnPaint(wxPaintEvent& WXUNUSED(event))
    {
        wxPaintDC dc(this);
        const wxSize s = GetClientSize();
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_bar->GetBackgroundColour()));
        dc.DrawRectangle(0, 0, s.x, s.y);

        const bool on = m_tool->IsToggled();
        const bool down = (m_pressed && m_hover) || on;
        const bool enabled = m_tool->IsEnabled() && m_bar->IsEnabled();
        if ( down || (m_hover && enabled) )
            ToolBevel(dc, wxRect(0, 0, s.x, s.y), down);
        const int nudge = down ? 1 : 0;

        // the picture, or its greyed copy
        wxBitmap bmp;
        if ( !m_bar->HasFlag(wxTB_NOICONS) )
        {
            bmp = enabled ? m_tool->GetNormalBitmap() : m_tool->GetDisabledBitmap();
#if wxUSE_IMAGE
            if ( !enabled && !bmp.IsOk() && m_tool->GetNormalBitmap().IsOk() )
                bmp = wxBitmap(m_tool->GetNormalBitmap().ConvertToImage().ConvertToDisabled());
#endif
        }
        wxString label;
        if ( m_bar->HasFlag(wxTB_TEXT) )
            label = wxStripMenuCodes(m_tool->GetLabel());
        int tw = 0, th = 0;
        if ( !label.empty() )
        {
            dc.SetFont(m_bar->GetFont());
            dc.GetTextExtent(label, &tw, &th);
        }
        const int bw = bmp.IsOk() ? bmp.GetWidth() : 0, bh = bmp.IsOk() ? bmp.GetHeight() : 0;
        int bx, by, tx, ty;
        if ( m_bar->HasFlag(wxTB_HORZ_LAYOUT) )
        {
            const int w = bw + (bw && tw ? TOOL_PAD : 0) + tw;
            bx = (s.x - w) / 2;
            by = (s.y - bh) / 2;
            tx = bx + bw + (bw ? TOOL_PAD : 0);
            ty = (s.y - th) / 2;
        }
        else
        {
            const int h = bh + (bh && th ? 1 : 0) + th;
            bx = (s.x - bw) / 2;
            by = (s.y - h) / 2;
            tx = (s.x - tw) / 2;
            ty = by + bh + (bh ? 1 : 0);
        }
        if ( bmp.IsOk() )
            dc.DrawBitmap(bmp, bx + nudge, by + nudge, true);
        if ( !label.empty() )
        {
            dc.SetTextForeground(wxSystemSettings::GetColour(enabled ? wxSYS_COLOUR_BTNTEXT : wxSYS_COLOUR_GRAYTEXT));
            dc.DrawText(label, tx + nudge, ty + nudge);
        }
    }

    void OnMouse(wxMouseEvent& event)
    {
        const wxEventType t = event.GetEventType();
        const bool inside = wxRect(GetClientSize()).Contains(event.GetPosition());
        const bool enabled = m_tool->IsEnabled() && m_bar->IsEnabled();
        if ( t == wxEVT_ENTER_WINDOW || t == wxEVT_LEAVE_WINDOW || t == wxEVT_MOTION )
        {
            if ( inside != m_hover )
            {
                m_hover = inside;
                Refresh();
                m_bar->OnMouseEnter(inside ? m_tool->GetId() : wxID_ANY);
            }
            return;
        }
        if ( !enabled )
            return;
        if ( t == wxEVT_LEFT_DOWN || t == wxEVT_LEFT_DCLICK )
        {
            m_pressed = true;
            m_hover = true;
            if ( !HasCapture() )
                CaptureMouse();
            Refresh();
            return;
        }
        if ( t == wxEVT_LEFT_UP )
        {
            const bool was = m_pressed;
            m_pressed = false;
            if ( HasCapture() )
                ReleaseMouse();
            m_hover = inside;
            Refresh();
            if ( was && inside )
                m_bar->OLToolClicked(m_tool);   // may delete this button
            return;
        }
        if ( t == wxEVT_RIGHT_UP && inside )
            m_bar->OnRightClick(m_tool->GetId(), event.GetX(), event.GetY());
    }

    void OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
    {
        m_pressed = false;
        Refresh();
    }

    wxToolBar *m_bar;
    wxToolBarToolBase *m_tool;
    bool m_hover, m_pressed;
};

// a tool: its button (made when it goes into the bar) or its control
class wxToolBarTool : public wxToolBarToolBase
{
public:
    wxToolBarTool(wxToolBar *tbar, int id, const wxString& label, const wxBitmapBundle& bmpNormal,
                  const wxBitmapBundle& bmpDisabled, wxItemKind kind, wxObject *clientData,
                  const wxString& shortHelp, const wxString& longHelp)
        : wxToolBarToolBase(tbar, id, label, bmpNormal, bmpDisabled, kind, clientData, shortHelp, longHelp),
          m_button(NULL)
    {
    }

    wxToolBarTool(wxToolBar *tbar, wxControl *control, const wxString& label)
        : wxToolBarToolBase(tbar, control, label), m_button(NULL)
    {
    }

    virtual ~wxToolBarTool()
    {
        DropButton();
    }

    void DropButton()
    {
        if ( m_button )
        {
            m_button->Destroy();
            m_button = NULL;
        }
    }

    // the window that stands for this tool in the bar
    wxWindow *GetWindow() const
    {
        if ( IsControl() )
            return GetControl();
        return m_button;
    }

    wxOLToolButton *m_button;
    wxPoint m_pos;          // where Realize put it
    wxSize m_size;
};
}

// ---- wxToolBar --------------------------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxToolBar, wxControl);

bool wxToolBar::Create(wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
                       long style, const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;
    FixupStyle();
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &wxToolBar::OnPaint, this);
    Bind(wxEVT_SIZE, &wxToolBar::OnSize, this);
    SetInitialSize(size);
    return true;
}

wxToolBarToolBase *wxToolBar::CreateTool(int toolid, const wxString& label, const wxBitmapBundle& bmpNormal,
                                         const wxBitmapBundle& bmpDisabled, wxItemKind kind, wxObject *clientData,
                                         const wxString& shortHelp, const wxString& longHelp)
{
    return new wxToolBarTool(this, toolid, label, bmpNormal, bmpDisabled, kind, clientData, shortHelp, longHelp);
}

wxToolBarToolBase *wxToolBar::CreateTool(wxControl *control, const wxString& label)
{
    return new wxToolBarTool(this, control, label);
}

bool wxToolBar::DoInsertTool(size_t WXUNUSED(pos), wxToolBarToolBase *toolBase)
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(toolBase);
    if ( tool->IsButton() && !tool->m_button )
        tool->m_button = new wxOLToolButton(this, tool);
    InvalidateBestSize();
    return true;
}

bool wxToolBar::DoDeleteTool(size_t WXUNUSED(pos), wxToolBarToolBase *toolBase)
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(toolBase);
    tool->DropButton();
    InvalidateBestSize();
    // the tools after it move up
    if ( !m_tools.empty() )
        Realize();
    Refresh();
    return true;
}

void wxToolBar::DoEnableTool(wxToolBarToolBase *toolBase, bool WXUNUSED(enable))
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(toolBase);
    if ( tool->m_button )
        tool->m_button->Refresh();
}

void wxToolBar::DoToggleTool(wxToolBarToolBase *toolBase, bool WXUNUSED(toggle))
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(toolBase);
    if ( tool->m_button )
        tool->m_button->Refresh();
}

void wxToolBar::DoSetToggle(wxToolBarToolBase *WXUNUSED(tool), bool WXUNUSED(toggle))
{
}

void wxToolBar::OLToolClicked(wxToolBarToolBase *tool)
{
    // a toggle or radio tool changes first, then the program hears of it;
    // a check tool the program refuses goes back
    bool toggled = false;
    if ( tool->CanBeToggled() )
    {
        if ( tool->GetKind() == wxITEM_RADIO )
        {
            if ( !tool->IsToggled() )
            {
                UnToggleRadioGroup(tool);
                tool->Toggle(true);
                DoToggleTool(tool, true);
            }
        }
        else
        {
            tool->Toggle();
            DoToggleTool(tool, tool->IsToggled());
        }
        toggled = tool->IsToggled();
    }
    const int id = tool->GetId();
    if ( !OnLeftClick(id, toggled) && tool->GetKind() == wxITEM_CHECK )
    {
        // the tool may be gone now: look it up again
        wxToolBarToolBase *again = FindById(id);
        if ( again && again->Toggle(!toggled) )
            DoToggleTool(again, !toggled);
    }
}

wxToolBarToolBase *wxToolBar::FindToolForPosition(wxCoord x, wxCoord y) const
{
    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        wxToolBarTool *tool = static_cast<wxToolBarTool *>(node->GetData());
        if ( wxRect(tool->m_pos, tool->m_size).Contains(x, y) )
            return tool;
    }
    return NULL;
}

void wxToolBar::SetWindowStyleFlag(long style)
{
    wxToolBarBase::SetWindowStyleFlag(style);
    if ( !m_tools.empty() )
        Realize();
}

void wxToolBar::SetToolShortHelp(int id, const wxString& helpString)
{
    wxToolBarBase::SetToolShortHelp(id, helpString);
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(FindById(id));
    if ( tool && tool->m_button )
        tool->m_button->UpdateTip();
}

void wxToolBar::SetToolNormalBitmap(int id, const wxBitmapBundle& bitmap)
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(FindById(id));
    if ( !tool )
        return;
    tool->SetNormalBitmap(bitmap);
    if ( tool->m_button )
        tool->m_button->Refresh();
}

void wxToolBar::SetToolDisabledBitmap(int id, const wxBitmapBundle& bitmap)
{
    wxToolBarTool *tool = static_cast<wxToolBarTool *>(FindById(id));
    if ( !tool )
        return;
    tool->SetDisabledBitmap(bitmap);
    if ( tool->m_button )
        tool->m_button->Refresh();
}

wxSize wxToolBar::OLLayout(bool place)
{
    const bool vertical = IsVertical();
    const int margin = vertical ? m_yMargin : m_xMargin;
    const int packing = wxMax(0, m_toolPacking);

    // each tool's size, and the bar's thickness
    int thick = 0, fixed = 0, stretch = 0;
    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        wxToolBarTool *tool = static_cast<wxToolBarTool *>(node->GetData());
        if ( tool->IsSeparator() )
            tool->m_size = vertical ? wxSize(0, SEP_SIZE) : wxSize(SEP_SIZE, 0);
        else if ( tool->IsControl() )
            tool->m_size = tool->GetControl()->GetBestSize();
        else if ( tool->m_button )
            tool->m_size = tool->m_button->Wanted();
        else
            tool->m_size = wxSize(0, 0);
        thick = wxMax(thick, vertical ? tool->m_size.x : tool->m_size.y);
        if ( tool->IsStretchableSpace() )
            stretch++;
        else
            fixed += (vertical ? tool->m_size.y : tool->m_size.x) + packing;
    }
    const int along = 2 * margin + fixed;
    const wxSize best = vertical ? wxSize(thick + 2 * m_xMargin + 2, along) : wxSize(along, thick + 2 * m_yMargin + 2);
    if ( !place )
        return best;

    // stretchable spaces share what is left
    const wxSize have = GetClientSize();
    int spare = (vertical ? have.y : have.x) - along;
    int at = margin;
    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        wxToolBarTool *tool = static_cast<wxToolBarTool *>(node->GetData());
        if ( tool->IsStretchableSpace() )
        {
            const int give = stretch > 0 && spare > 0 ? spare / stretch : 0;
            spare -= give;
            stretch--;
            tool->m_size = vertical ? wxSize(thick, give) : wxSize(give, thick);
        }
        wxSize s = tool->m_size;
        // buttons and separators fill the bar's thickness; a control
        // keeps its own and is centred
        if ( !tool->IsControl() )
            (vertical ? s.x : s.y) = thick;
        const int across = (vertical ? m_xMargin : m_yMargin) + 1 + (thick - (vertical ? s.x : s.y)) / 2;
        tool->m_pos = vertical ? wxPoint(across, at) : wxPoint(at, across);
        tool->m_size = s;
        if ( wxWindow *w = tool->GetWindow() )
            w->SetSize(wxRect(tool->m_pos, s));
        at += (vertical ? s.y : s.x) + (tool->IsStretchableSpace() ? 0 : packing);
    }
    return best;
}

wxSize wxToolBar::DoGetBestSize() const
{
    return const_cast<wxToolBar *>(this)->OLLayout(false);
}

bool wxToolBar::Realize()
{
    if ( !wxToolBarBase::Realize() )
        return false;
    InvalidateBestSize();
    const wxSize best = GetBestSize();
    // a frame's bar is placed by the frame; one in a sizer by the sizer
    wxFrame *frame = wxDynamicCast(GetParent(), wxFrame);
    if ( frame && frame->GetToolBar() == this )
    {
        if ( IsVertical() )
            SetSize(best.x, -1, wxSIZE_USE_EXISTING);
        else
            SetSize(-1, best.y, wxSIZE_USE_EXISTING);
        frame->SendSizeEvent();
    }
    else
    {
        SetMinSize(best);
        if ( GetSize().x < best.x || GetSize().y < best.y )
            SetSize(wxSize(wxMax(GetSize().x, best.x), wxMax(GetSize().y, best.y)));
    }
    OLLayout(true);
    Refresh();
    return true;
}

void wxToolBar::OnSize(wxSizeEvent& event)
{
    OLLayout(true);
    Refresh();
    event.Skip();
}

void wxToolBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    const wxSize s = GetClientSize();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(0, 0, s.x, s.y);
    const wxColour light = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNHIGHLIGHT);
    const wxColour dark = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
    // a line between the bar and what is below it (or beside it)
    if ( !HasFlag(wxTB_NODIVIDER) )
    {
        dc.SetPen(wxPen(dark));
        if ( IsVertical() )
            dc.DrawLine(s.x - 1, 0, s.x - 1, s.y);
        else
            dc.DrawLine(0, s.y - 1, s.x, s.y - 1);
    }
    // separators: a groove
    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        wxToolBarTool *tool = static_cast<wxToolBarTool *>(node->GetData());
        if ( !tool->IsSeparator() || tool->IsStretchable() )
            continue;
        const wxRect r(tool->m_pos, tool->m_size);
        if ( IsVertical() )
        {
            const int y = r.y + r.height / 2;
            dc.SetPen(wxPen(dark));
            dc.DrawLine(r.x + 2, y, r.GetRight() - 1, y);
            dc.SetPen(wxPen(light));
            dc.DrawLine(r.x + 2, y + 1, r.GetRight() - 1, y + 1);
        }
        else
        {
            const int x = r.x + r.width / 2;
            dc.SetPen(wxPen(dark));
            dc.DrawLine(x, r.y + 2, x, r.GetBottom() - 1);
            dc.SetPen(wxPen(light));
            dc.DrawLine(x + 1, r.y + 2, x + 1, r.GetBottom() - 1);
        }
    }
}

#endif // wxUSE_TOOLBAR
