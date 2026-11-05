/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/clipbrd.cpp
// Purpose:     wxClipboard (text on clipboard.device unit 0 as IFF FTXT,
//              other formats within the program), wxDataFormat,
//              wxDataObject, the bitmap and file data objects, drag and
//              drop within a window, and wxToolTip (olui's title)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/image.h"
    #include "wx/log.h"
#endif

#include "wx/clipbrd.h"
#include "wx/dataobj.h"
#include "wx/dnd.h"
#include "wx/tooltip.h"
#include "wx/mstream.h"
#include "wx/evtloop.h"

#include <exec/ports.h>
#include <exec/io.h>
#include <devices/clipboard.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include "private.h"

// ---- wxDataFormat ----------------------------------------------------------------------

#if wxUSE_DATAOBJ

void wxDataFormat::SetType(wxDataFormatId type)
{
    m_type = type;
    m_format = (NativeFormat)type;
    m_id.clear();
}

void wxDataFormat::SetId(NativeFormat format)
{
    m_format = format;
    m_type = format < wxDF_MAX ? (wxDataFormatId)format : wxDF_PRIVATE;
}

void wxDataFormat::SetId(const wxString& id)
{
    // a private format: a number of its own from its name
    m_type = wxDF_PRIVATE;
    m_id = id;
    unsigned long h = 5381;
    for ( wxString::const_iterator i = id.begin(); i != id.end(); ++i )
        h = h * 33 + (unsigned long)(wxChar)*i;
    m_format = 0x10000 + (h & 0xffffff);
}

// ---- wxDataObject ------------------------------------------------------------------------

bool wxDataObject::IsSupportedFormat(const wxDataFormat& format, Direction dir) const
{
    const size_t n = GetFormatCount(dir);
    if ( n == 1 )
        return format == GetPreferredFormat(dir);
    wxDataFormat *formats = new wxDataFormat[n];
    GetAllFormats(formats, dir);
    bool found = false;
    for ( size_t i = 0; i < n && !found; i++ )
        found = formats[i] == format;
    delete[] formats;
    return found;
}

// a bitmap: PNG in memory
size_t wxBitmapDataObject::GetDataSize() const
{
#if wxUSE_IMAGE && wxUSE_LIBPNG
    if ( !m_bitmap.IsOk() )
        return 0;
    wxMemoryOutputStream out;
    if ( !m_bitmap.ConvertToImage().SaveFile(out, wxBITMAP_TYPE_PNG) )
        return 0;
    m_png.SetDataLen(0);
    m_png.AppendData(out.GetOutputStreamBuffer()->GetBufferStart(), out.GetSize());
    return m_png.GetDataLen();
#else
    return 0;
#endif
}

bool wxBitmapDataObject::GetDataHere(void *buf) const
{
    if ( !m_png.GetDataLen() && !GetDataSize() )
        return false;
    memcpy(buf, m_png.GetData(), m_png.GetDataLen());
    return true;
}

bool wxBitmapDataObject::SetData(size_t len, const void *buf)
{
#if wxUSE_IMAGE && wxUSE_LIBPNG
    wxMemoryInputStream in(buf, len);
    wxImage img;
    if ( !img.LoadFile(in, wxBITMAP_TYPE_PNG) )
        return false;
    m_bitmap = wxBitmap(img);
    return m_bitmap.IsOk();
#else
    (void)len;
    (void)buf;
    return false;
#endif
}

// files: their paths in UTF-8, a line each
size_t wxFileDataObject::GetDataSize() const
{
    size_t n = 0;
    for ( size_t i = 0; i < m_filenames.size(); i++ )
        n += wxOLToUTF8(m_filenames[i]).size() + 1;
    return n;
}

bool wxFileDataObject::GetDataHere(void *buf) const
{
    char *p = (char *)buf;
    for ( size_t i = 0; i < m_filenames.size(); i++ )
    {
        const std::string s = wxOLToUTF8(m_filenames[i]);
        memcpy(p, s.c_str(), s.size());
        p += s.size();
        *p++ = '\n';
    }
    return true;
}

bool wxFileDataObject::SetData(size_t len, const void *buf)
{
    m_filenames.clear();
    std::string all((const char *)buf, len);
    size_t at = 0;
    while ( at < all.size() )
    {
        size_t nl = all.find('\n', at);
        if ( nl == std::string::npos )
            nl = all.size();
        if ( nl > at )
            m_filenames.Add(wxOLFromUTF8(all.substr(at, nl - at).c_str()));
        at = nl + 1;
    }
    return true;
}

#endif // wxUSE_DATAOBJ

// ---- clipboard.device ------------------------------------------------------------------------

#if wxUSE_CLIPBOARD

namespace
{
struct Clip
{
    struct MsgPort *port;
    struct IOClipReq *io;

    Clip() : port(NULL), io(NULL)
    {
        if ( !(port = CreateMsgPort()) )
            return;
        io = (struct IOClipReq *)CreateIORequest(port, sizeof(struct IOClipReq));
        if ( io && OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)io, 0) != 0 )
        {
            DeleteIORequest((struct IORequest *)io);
            io = NULL;
        }
    }
    ~Clip()
    {
        if ( io )
        {
            CloseDevice((struct IORequest *)io);
            DeleteIORequest((struct IORequest *)io);
        }
        if ( port )
            DeleteMsgPort(port);
    }
    bool Ok() const { return io != NULL; }

    bool Write(const void *data, ULONG len)
    {
        io->io_Command = CMD_WRITE;
        io->io_Data = (STRPTR)data;
        io->io_Length = len;
        DoIO((struct IORequest *)io);
        return io->io_Error == 0 && io->io_Actual == len;
    }
    ULONG Read(void *data, ULONG len)
    {
        io->io_Command = CMD_READ;
        io->io_Data = (STRPTR)data;
        io->io_Length = len;
        DoIO((struct IORequest *)io);
        return io->io_Error ? 0 : io->io_Actual;
    }
    // reading to the end lets the device go
    void ReadToEnd()
    {
        char buf[64];
        while ( Read(buf, sizeof buf) > 0 )
            ;
    }
};

// text to unit 0: FORM FTXT with one CHRS chunk, Latin-1
bool PutText(const wxString& text)
{
    Clip c;
    if ( !c.Ok() )
        return false;
    std::string s = wxOLToLatin1(text);
    const ULONG len = (ULONG)s.size(), pad = len & 1;
    const ULONG formLen = 4 + 8 + len + pad;
    ULONG head[5] = { 0x464f524dUL, formLen, 0x46545854UL, 0x43485253UL, len };   // FORM n FTXT CHRS n
    c.io->io_Offset = 0;
    c.io->io_Error = 0;
    c.io->io_ClipID = 0;
    bool ok = c.Write(head, sizeof head) && (len == 0 || c.Write(s.data(), len)) && (!pad || c.Write("", 1));
    c.io->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)c.io);
    return ok;
}

// unit 0's text, when it holds FTXT
bool GetText(wxString *text)
{
    Clip c;
    if ( !c.Ok() )
        return false;
    c.io->io_Offset = 0;
    c.io->io_Error = 0;
    c.io->io_ClipID = 0;
    ULONG head[3];
    if ( c.Read(head, sizeof head) != sizeof head || head[0] != 0x464f524dUL || head[2] != 0x46545854UL )
    {
        c.ReadToEnd();
        return false;
    }
    std::string all;
    ULONG left = head[1] > 4 ? head[1] - 4 : 0;
    while ( left >= 8 )
    {
        ULONG ch[2];
        if ( c.Read(ch, sizeof ch) != sizeof ch )
            break;
        left -= 8;
        const ULONG size = ch[1], padded = size + (size & 1);
        if ( ch[0] == 0x43485253UL && text )
        {
            std::string part(size, '\0');
            if ( size && c.Read(&part[0], size) != size )
                break;
            if ( size & 1 )
            {
                char p;
                c.Read(&p, 1);
            }
            all += part;
        }
        else
        {
            // a chunk it skips (FONS, a CSET)
            std::string skip(padded, '\0');
            if ( padded && c.Read(&skip[0], padded) != padded )
                break;
        }
        left = left > padded ? left - padded : 0;
    }
    c.ReadToEnd();
    if ( text )
    {
        // Latin-1: a character a byte
        wxString s;
        s.reserve(all.size());
        for ( size_t i = 0; i < all.size(); i++ )
            s += (wxChar)(unsigned char)all[i];
        *text = s;
    }
    return true;
}

bool HasText()
{
    Clip c;
    if ( !c.Ok() )
        return false;
    c.io->io_Offset = 0;
    c.io->io_Error = 0;
    c.io->io_ClipID = 0;
    ULONG head[3];
    const bool ok = c.Read(head, sizeof head) == sizeof head && head[0] == 0x464f524dUL && head[2] == 0x46545854UL;
    c.ReadToEnd();
    return ok;
}
}

// ---- wxClipboard ------------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxClipboard, wxObject);

wxClipboard::wxClipboard() : m_olData(NULL), m_open(false)
{
}

wxClipboard::~wxClipboard()
{
    delete m_olData;
}

bool wxClipboard::Open()
{
    wxCHECK_MSG(!m_open, false, "the clipboard is already open");
    m_open = true;
    return true;
}

void wxClipboard::Close()
{
    m_open = false;
}

bool wxClipboard::SetData(wxDataObject *data)
{
    Clear();
    return AddData(data);
}

bool wxClipboard::AddData(wxDataObject *data)
{
    wxCHECK_MSG(data, false, "no data");
    delete m_olData;
    m_olData = data;
    // its text goes on the Amiga's clipboard, for every program
    const wxDataFormat text(wxDF_UNICODETEXT);
    if ( data->IsSupported(text, wxDataObject::Get) || data->IsSupported(wxDF_TEXT, wxDataObject::Get) )
    {
        wxTextDataObject t;
        const wxDataFormat f = data->IsSupported(text, wxDataObject::Get) ? text : wxDataFormat(wxDF_TEXT);
        const size_t n = data->GetDataSize(f);
        wxCharBuffer buf(n);
        if ( data->GetDataHere(f, buf.data()) && t.SetData(f, n, buf.data()) )
            PutText(t.GetText());
    }
    return true;
}

bool wxClipboard::GetData(wxDataObject& data)
{
    const wxDataFormat text(wxDF_UNICODETEXT);
    const bool wantsText = data.IsSupported(text, wxDataObject::Set) || data.IsSupported(wxDF_TEXT, wxDataObject::Set);
    wxString sysText;
    // the Amiga's clipboard has text another program put there
    if ( wantsText && GetText(&sysText) )
    {
        bool ours = false;
        if ( m_olData && m_olData->IsSupported(text, wxDataObject::Get) )
        {
            wxTextDataObject t;
            const size_t n = m_olData->GetDataSize(text);
            wxCharBuffer buf(n);
            ours = m_olData->GetDataHere(text, buf.data()) && t.SetData(text, n, buf.data()) &&
                   t.GetText() == sysText;
        }
        if ( !ours )
        {
            wxTextDataObject t(sysText);
            const wxDataFormat f = data.IsSupported(text, wxDataObject::Set) ? text : wxDataFormat(wxDF_TEXT);
            const size_t n = t.GetDataSize(f);
            wxCharBuffer buf(n);
            return t.GetDataHere(f, buf.data()) && data.SetData(f, n, buf.data());
        }
    }
    if ( !m_olData )
        return false;
    // the formats both have, in the order data prefers
    const size_t count = data.GetFormatCount(wxDataObject::Set);
    wxDataFormat *formats = new wxDataFormat[count];
    data.GetAllFormats(formats, wxDataObject::Set);
    bool ok = false;
    for ( size_t i = 0; i < count && !ok; i++ )
    {
        if ( !m_olData->IsSupported(formats[i], wxDataObject::Get) )
            continue;
        const size_t n = m_olData->GetDataSize(formats[i]);
        wxCharBuffer buf(n);
        ok = m_olData->GetDataHere(formats[i], buf.data()) && data.SetData(formats[i], n, buf.data());
    }
    delete[] formats;
    return ok;
}

void wxClipboard::Clear()
{
    delete m_olData;
    m_olData = NULL;
}

bool wxClipboard::IsSupported(const wxDataFormat& format)
{
    if ( m_olData && m_olData->IsSupported(format, wxDataObject::Get) )
        return true;
    if ( format == wxDF_TEXT || format == wxDF_UNICODETEXT )
        return HasText();
    return false;
}

#endif // wxUSE_CLIPBOARD

// ---- drag and drop: within the program ------------------------------------------------------------

#if wxUSE_DRAG_AND_DROP

wxDataFormat wxDropTarget::GetMatchingPair()
{
    if ( !m_dataObject || !m_olSource )
        return wxDataFormat();
    const size_t n = m_olSource->GetFormatCount(wxDataObject::Get);
    wxDataFormat *formats = new wxDataFormat[n];
    m_olSource->GetAllFormats(formats, wxDataObject::Get);
    wxDataFormat found;
    for ( size_t i = 0; i < n; i++ )
        if ( m_dataObject->IsSupported(formats[i], wxDataObject::Set) )
        {
            found = formats[i];
            break;
        }
    delete[] formats;
    return found;
}

bool wxDropTarget::OnDrop(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y))
{
    return true;
}

wxDragResult wxDropTarget::OnData(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y), wxDragResult def)
{
    return GetData() ? def : wxDragNone;
}

bool wxDropTarget::GetData()
{
    const wxDataFormat f = GetMatchingPair();
    if ( f.GetType() == wxDF_INVALID )
        return false;
    const size_t n = m_olSource->GetDataSize(f);
    wxCharBuffer buf(n);
    return m_olSource->GetDataHere(f, buf.data()) && m_dataObject->SetData(f, n, buf.data());
}

wxDropSource::wxDropSource(wxWindow *win, const wxCursor& copy, const wxCursor& move, const wxCursor& none)
    : wxDropSourceBase(copy, move, none), m_win(win)
{
}

wxDropSource::wxDropSource(wxDataObject& data, wxWindow *win, const wxCursor& copy, const wxCursor& move,
                           const wxCursor& none)
    : wxDropSourceBase(copy, move, none), m_win(win)
{
    SetData(data);
}

wxDragResult wxDropSource::DoDragDrop(int flags)
{
    // the button is still down: the window under the pointer when it goes
    // up gets the drop, if it has a target taking the data
    if ( !m_data || !m_win )
        return wxDragNone;
    wxWindow *start = m_win;
    wxWindow *over = NULL;
    while ( wxGetMouseState().LeftIsDown() )
    {
        wxPoint p = wxGetMousePosition();
        over = wxFindWindowAtPoint(p);
        if ( wxEventLoopBase *loop = wxEventLoopBase::GetActive() )
            loop->DispatchTimeout(20);
        else
            break;
    }
    if ( !over )
        over = wxFindWindowAtPoint(wxGetMousePosition());
    for ( wxWindow *w = over; w; w = w->GetParent() )
    {
        wxDropTarget *t = w->GetDropTarget();
        if ( !t )
            continue;
        t->OLSetSource(m_data);
        wxPoint p = w->ScreenToClient(wxGetMousePosition());
        const wxDragResult def = (flags & wxDrag_DefaultMove) ? wxDragMove : wxDragCopy;
        wxDragResult r = wxDragNone;
        if ( t->OnDrop(p.x, p.y) )
            r = t->OnData(p.x, p.y, def);
        t->OLSetSource(NULL);
        (void)start;
        return r;
    }
    return wxDragNone;
}

#endif // wxUSE_DRAG_AND_DROP

// ---- wxToolTip: the element's title, which olui shows ----------------------------------------------

#if wxUSE_TOOLTIPS

wxIMPLEMENT_ABSTRACT_CLASS(wxToolTip, wxObject);

void wxToolTip::Enable(bool WXUNUSED(flag)) {}
void wxToolTip::SetDelay(long WXUNUSED(milliseconds)) {}
void wxToolTip::SetAutoPop(long WXUNUSED(milliseconds)) {}
void wxToolTip::SetReshow(long WXUNUSED(milliseconds)) {}
void wxToolTip::SetMaxWidth(int WXUNUSED(width)) {}

void wxToolTip::SetTip(const wxString& tip)
{
    if ( tip == m_text )
        return;
    m_text = tip;
    if ( m_window )
        m_window->OLRecreate();
}

#endif // wxUSE_TOOLTIPS

#if wxUSE_DATAOBJ && wxUSE_UNICODE
// ----------------------------------------------------------------------------
// wxTextDataObject: offer Unicode text first, then plain text
// ----------------------------------------------------------------------------

void wxTextDataObject::GetAllFormats(wxDataFormat *formats,
                                     wxDataObjectBase::Direction WXUNUSED(dir)) const
{
    *formats++ = wxDataFormat(wxDF_UNICODETEXT);
    *formats = wxDataFormat(wxDF_TEXT);
}
#endif
