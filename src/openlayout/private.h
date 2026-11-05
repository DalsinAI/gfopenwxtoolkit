/////////////////////////////////////////////////////////////////////////////
// Name:        src/openlayout/private.h
// Purpose:     what the openlayout port's sources share: the screen,
//              cybergraphics, pens, text in the Amiga's encodings, markup
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_PRIVATE_H_
#define _WX_OPENLAYOUT_PRIVATE_H_

#include <exec/types.h>
#include <inline/macros.h>

// The port calls the Amiga's libraries through clib's prototypes and
// libamiga's stubs, not proto/'s inline macros: those are macros named as
// wx's methods are (SetFont, Move, Text, Draw ...).
struct GfxBase;
struct IntuitionBase;
extern "C" struct GfxBase *GfxBase;
extern "C" struct IntuitionBase *IntuitionBase;
#include <string>

extern "C" {
#include "gfopenlayout_ui.h"
#include "gfopenlayout_ui_amiga.h"
}

struct Screen;
struct RastPort;
struct BitMap;
struct Library;
struct TextFont;
struct Window;

// ---- cybergraphics.library, opened the first time it is asked for -------
extern struct Library *wxOLCyberGfxBase;
bool wxOLHaveCgx();     // opened, version 41 or later

#define wxOL_RECTFMT_ARGB 2
#define wxOL_CYBRMATTR_DEPTH 0x80000007UL
#define wxOL_CYBRMATTR_ISCYBERGFX 0x80000008UL

#define wxOL_CGX_FILL(rp, x, y, w, h, argb) LP6(0x96, ULONG, FillPixelArray, struct RastPort *, (rp), a1, UWORD, (x), d0, \
    UWORD, (y), d1, UWORD, (w), d2, UWORD, (h), d3, ULONG, (argb), d4, , wxOLCyberGfxBase)
#define wxOL_CGX_MAPATTR(bm, a) LP2(0x60, ULONG, GetCyberMapAttr, struct BitMap *, (bm), a0, ULONG, (a), d0, , wxOLCyberGfxBase)
#define wxOL_CGX_WRITE(buf, sx, sy, mod, rp, x, y, w, h) LP10(0x7e, ULONG, WritePixelArray, APTR, (buf), a0, UWORD, (sx), d0, \
    UWORD, (sy), d1, UWORD, (mod), d2, struct RastPort *, (rp), a1, UWORD, (x), d3, UWORD, (y), d4, UWORD, (w), d5, \
    UWORD, (h), d6, UBYTE, wxOL_RECTFMT_ARGB, d7, , wxOLCyberGfxBase)
#define wxOL_CGX_READ(buf, sx, sy, mod, rp, x, y, w, h) LP10(0x78, ULONG, ReadPixelArray, APTR, (buf), a0, UWORD, (sx), d0, \
    UWORD, (sy), d1, UWORD, (mod), d2, struct RastPort *, (rp), a1, UWORD, (x), d3, UWORD, (y), d4, UWORD, (w), d5, \
    UWORD, (h), d6, UBYTE, wxOL_RECTFMT_ARGB, d7, , wxOLCyberGfxBase)
#define wxOL_CGX_WRITEALPHA(buf, sx, sy, mod, rp, x, y, w, h, ga) LP10(0xd8, ULONG, WritePixelArrayAlpha, APTR, (buf), a0, \
    UWORD, (sx), d0, UWORD, (sy), d1, UWORD, (mod), d2, struct RastPort *, (rp), a1, UWORD, (x), d3, UWORD, (y), d4, \
    UWORD, (w), d5, UWORD, (h), d6, ULONG, (ga), d7, , wxOLCyberGfxBase)

// a BitMap (a window's, a buffer's) cybergraphics draws in true colour
bool wxOLIsTrueColour(struct BitMap *bm);

// ---- the screen the program's windows open on ----------------------------
// the default public screen, locked from wxApp::Initialize to CleanUp
struct Screen *wxOLGetScreen();
void wxOLReleaseScreen();

// ---- pens: a colour's nearest pen on the screen, obtained once ----------
long wxOLPenFor(unsigned long rgb);
void wxOLReleasePens();

// ---- text ------------------------------------------------------------------
std::string wxOLToUTF8(const wxString& s);
wxString wxOLFromUTF8(const char *s);
// the Amiga's own 8-bit text (Latin-1): what the RastPort's fonts draw
std::string wxOLToLatin1(const wxString& s);
// text made safe in markup: & < > " written as entities
wxString wxOLEscape(const wxString& s);
// a label's & mnemonics taken out (&& is one &)
wxString wxOLStripMnemonics(const wxString& s);

// ---- the screen's font, wx's default GUI font -----------------------------
struct TextFont *wxOLScreenFont();
// the measure of text in a font (TextLength, the font's height and baseline)
void wxOLTextExtent(struct TextFont *tf, unsigned style, const wxString& s, int *w, int *h, int *descent);

// the qualifiers of the last input event a window had (the event loop sets them)
extern unsigned short wxOLLastQualifier;

// ---- the windows ---------------------------------------------------------------
// the event loop's: every open top-level window's Intuition window shares
// one message port (opened at the first)
struct MsgPort *wxOLSharedPort();
void wxOLFreeSharedPort();
// the signal threads and timers use to wake the loop (main task's)
unsigned long wxOLWakeSignal();
void wxOLWakeMain();

#endif // _WX_OPENLAYOUT_PRIVATE_H_
