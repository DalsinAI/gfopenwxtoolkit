/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/sound.cpp
// Purpose:     wxSound backend for wx's unix wxSound: plays through ahi.device
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_SOUND

#include "wx/sound.h"

#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <proto/exec.h>

namespace
{

// ahi.device's request (AHI's devices/ahi.h, interface version 4)
struct OLAhiRequest
{
    struct IOStdReq ahir_Std;
    UWORD ahir_Version;
    UWORD ahir_Pad1;
    ULONG ahir_Private[2];
    ULONG ahir_Type;
    ULONG ahir_Frequency;
    LONG ahir_Volume;
    LONG ahir_Position;
    struct OLAhiRequest *ahir_Link;
};

const ULONG OL_AHIST_M8S = 0, OL_AHIST_M16S = 1, OL_AHIST_S8S = 2,
            OL_AHIST_S16S = 3;
const ULONG OL_CHUNK = 16384;   // bytes each request plays

// Turns a slice of WAV data (little-endian 16-bit or unsigned 8-bit) into
// what AHI wants (big-endian 16-bit or signed 8-bit).
void OLConvert(UBYTE *to, const wxUint8 *from, size_t bytes, unsigned bits)
{
    if ( bits == 16 )
    {
        for ( size_t i = 0; i + 1 < bytes; i += 2 )
        {
            to[i] = from[i + 1];
            to[i + 1] = from[i];
        }
    }
    else
    {
        for ( size_t i = 0; i < bytes; i++ )
            to[i] = (UBYTE)(from[i] ^ 0x80);
    }
}

} // anonymous namespace

class wxSoundBackendAHI : public wxSoundBackend
{
public:
    wxString GetName() const wxOVERRIDE { return wxT("AHI"); }
    int GetPriority() const wxOVERRIDE { return 10; }
    bool IsAvailable() const wxOVERRIDE;
    bool HasNativeAsyncPlayback() const wxOVERRIDE { return false; }
    bool Play(wxSoundData *data, unsigned flags,
              volatile wxSoundPlaybackStatus *status) wxOVERRIDE;
    void Stop() wxOVERRIDE {}
    bool IsPlaying() const wxOVERRIDE { return false; }
};

bool wxSoundBackendAHI::IsAvailable() const
{
    struct MsgPort *port = CreateMsgPort();
    if ( !port )
        return false;
    OLAhiRequest *req = (OLAhiRequest *)CreateIORequest(port, sizeof(OLAhiRequest));
    bool ok = false;
    if ( req )
    {
        req->ahir_Version = 4;
        if ( OpenDevice((CONST_STRPTR)"ahi.device", 0, (struct IORequest *)req, 0) == 0 )
        {
            ok = true;
            CloseDevice((struct IORequest *)req);
        }
        DeleteIORequest((struct IORequest *)req);
    }
    DeleteMsgPort(port);
    return ok;
}

bool wxSoundBackendAHI::Play(wxSoundData *data, unsigned flags,
                             volatile wxSoundPlaybackStatus *status)
{
    if ( !data || !data->m_data || !data->m_dataBytes ||
         (data->m_bitsPerSample != 8 && data->m_bitsPerSample != 16) ||
         data->m_channels < 1 || data->m_channels > 2 )
        return false;

    const unsigned frame = data->m_channels * data->m_bitsPerSample / 8;
    const ULONG type = data->m_channels == 2
        ? (data->m_bitsPerSample == 16 ? OL_AHIST_S16S : OL_AHIST_S8S)
        : (data->m_bitsPerSample == 16 ? OL_AHIST_M16S : OL_AHIST_M8S);
    const ULONG chunk = OL_CHUNK - OL_CHUNK % frame;

    struct MsgPort *port = CreateMsgPort();
    if ( !port )
        return false;
    OLAhiRequest *req[2] = { NULL, NULL };
    UBYTE *buf[2] = { NULL, NULL };
    bool opened = false, ok = false;

    req[0] = (OLAhiRequest *)CreateIORequest(port, sizeof(OLAhiRequest));
    req[1] = (OLAhiRequest *)AllocVec(sizeof(OLAhiRequest), MEMF_PUBLIC | MEMF_CLEAR);
    buf[0] = (UBYTE *)AllocVec(chunk, MEMF_PUBLIC);
    buf[1] = (UBYTE *)AllocVec(chunk, MEMF_PUBLIC);
    if ( req[0] && req[1] && buf[0] && buf[1] )
    {
        req[0]->ahir_Version = 4;
        opened = OpenDevice((CONST_STRPTR)"ahi.device", 0,
                            (struct IORequest *)req[0], 0) == 0;
    }

    if ( opened )
    {
        // two requests, each linked to the one before, so AHI plays them
        // back to back while the next slice is being filled
        CopyMem(req[0], req[1], sizeof(OLAhiRequest));
        OLAhiRequest *link = NULL;
        bool pending[2] = { false, false };
        size_t pos = 0;
        int cur = 0;
        ok = true;
        do
        {
            while ( pos < data->m_dataBytes && !status->m_stopRequested )
            {
                if ( pending[cur] )
                {
                    WaitIO((struct IORequest *)req[cur]);
                    pending[cur] = false;
                }
                ULONG n = data->m_dataBytes - pos;
                if ( n > chunk )
                    n = chunk;
                n -= n % frame;
                if ( !n )
                    break;
                OLConvert(buf[cur], data->m_data + pos, n, data->m_bitsPerSample);
                pos += n;

                OLAhiRequest *r = req[cur];
                r->ahir_Std.io_Command = CMD_WRITE;
                r->ahir_Std.io_Data = buf[cur];
                r->ahir_Std.io_Length = n;
                r->ahir_Std.io_Offset = 0;
                r->ahir_Type = type;
                r->ahir_Frequency = data->m_samplingRate;
                r->ahir_Volume = 0x10000;
                r->ahir_Position = 0x8000;
                r->ahir_Link = link;
                SendIO((struct IORequest *)r);
                pending[cur] = true;
                link = r;
                cur ^= 1;
            }
            if ( status->m_stopRequested )
            {
                for ( int i = 0; i < 2; i++ )
                    if ( pending[i] )
                        AbortIO((struct IORequest *)req[i]);
            }
            for ( int i = 0; i < 2; i++ )
                if ( pending[i] )
                {
                    WaitIO((struct IORequest *)req[i]);
                    pending[i] = false;
                }
            link = NULL;
            pos = 0;
        }
        while ( (flags & wxSOUND_LOOP) && !status->m_stopRequested );

        CloseDevice((struct IORequest *)req[0]);
    }

    if ( buf[0] ) FreeVec(buf[0]);
    if ( buf[1] ) FreeVec(buf[1]);
    if ( req[1] ) FreeVec(req[1]);
    if ( req[0] ) DeleteIORequest((struct IORequest *)req[0]);
    DeleteMsgPort(port);
    return ok;
}

wxSoundBackend *wxCreateSoundBackendAHI()
{
    return new wxSoundBackendAHI;
}

#endif // wxUSE_SOUND
