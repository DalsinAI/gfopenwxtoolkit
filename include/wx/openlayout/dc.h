/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/dc.h
// Purpose:     wxDCImpl drawing through graphics.library into a RastPort
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DC_H_
#define _WX_OPENLAYOUT_DC_H_

#include "wx/openlayout/ndktypes.h"

#include "wx/dc.h"

struct RastPort;
struct TextFont;
class wxOLPens;

// Every DC of the port draws into a RastPort at an origin: a window's paint
// DC into olui's buffer where the window's picture is, a memory DC into
// its bitmap's BitMap. Colours: cybergraphics' FillPixelArray for filled
// boxes on a true-colour screen, otherwise pens obtained for the colour.
class WXDLLIMPEXP_CORE wxOLDCImpl : public wxDCImpl
{
public:
    wxOLDCImpl(wxDC *owner);
    virtual ~wxOLDCImpl();

    virtual bool IsOk() const wxOVERRIDE { return m_rp != NULL; }

    virtual void Clear() wxOVERRIDE;
    virtual bool StartDoc(const wxString& WXUNUSED(message)) wxOVERRIDE { return true; }
    virtual void EndDoc() wxOVERRIDE {}
    virtual void StartPage() wxOVERRIDE {}
    virtual void EndPage() wxOVERRIDE {}

    virtual void SetFont(const wxFont& font) wxOVERRIDE;
    virtual void SetPen(const wxPen& pen) wxOVERRIDE;
    virtual void SetBrush(const wxBrush& brush) wxOVERRIDE;
    virtual void SetBackground(const wxBrush& brush) wxOVERRIDE;
    virtual void SetBackgroundMode(int mode) wxOVERRIDE;
#if wxUSE_PALETTE
    virtual void SetPalette(const wxPalette& WXUNUSED(palette)) wxOVERRIDE {}
#endif
    virtual void SetLogicalFunction(wxRasterOperationMode function) wxOVERRIDE;
    virtual void DestroyClippingRegion() wxOVERRIDE;

    virtual wxCoord GetCharHeight() const wxOVERRIDE;
    virtual wxCoord GetCharWidth() const wxOVERRIDE;
    virtual void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL, wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const wxOVERRIDE;
    virtual bool DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const wxOVERRIDE;

    virtual bool CanDrawBitmap() const wxOVERRIDE { return true; }
    virtual bool CanGetTextExtent() const wxOVERRIDE { return true; }
    virtual int GetDepth() const wxOVERRIDE;
    virtual wxSize GetPPI() const wxOVERRIDE;

    // the port's own
    struct RastPort *OLGetRastPort() const { return m_rp; }
    int OLGetOriginX() const { return m_ox; }
    int OLGetOriginY() const { return m_oy; }

protected:
    // where it draws: rp at device 0,0 being ox, oy of rp, w by h, cut to
    // clip (rp's pixels; empty: the w by h box)
    void OLInit(struct RastPort *rp, int ox, int oy, int w, int h);

    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE) wxOVERRIDE;
    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const wxOVERRIDE;
    virtual void DoDrawPoint(wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE;
    virtual void DoDrawArc(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                           wxCoord xc, wxCoord yc) wxOVERRIDE;
    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                   double sa, double ea) wxOVERRIDE;
    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height,
                                        double radius) wxOVERRIDE;
    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    virtual void DoCrossHair(wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawBitmap(const wxBitmap& bmp, wxCoord x, wxCoord y,
                              bool useMask = false) wxOVERRIDE;
    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y) wxOVERRIDE;
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                   double angle) wxOVERRIDE;
    virtual bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                        wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord) wxOVERRIDE;
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    virtual void DoSetDeviceClippingRegion(const wxRegion& region) wxOVERRIDE;
    virtual void DoGetSize(int *width, int *height) const wxOVERRIDE;
    virtual void DoGetSizeMM(int *width, int *height) const wxOVERRIDE;
    virtual void DoDrawLines(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset) wxOVERRIDE;
    virtual void DoDrawPolygon(int n, const wxPoint points[], wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;

    // a bitmap's pixels (sx, sy, w, h of it) to device dx, dy, the mask or
    // alpha kept when useMask
    void OLPutBitmap(const wxBitmap& bmp, int sx, int sy, int w, int h, int dx, int dy, bool useMask);
    // the device box cut to the clip; false when nothing is left
    bool OLCut(int& x, int& y, int& w, int& h) const;
    // the colour for drawing: true-colour fill, or a pen made the APen
    void OLFill(int x, int y, int w, int h, const wxColour& c);
    void OLSetAPen(const wxColour& c);
    void OLApplyClip();
    void OLRemoveClip();
    struct TextFont *OLFont() const;

    struct RastPort *m_rp;
    int m_ox, m_oy, m_w, m_h;
    wxRect m_olClip;             // device coordinates
    bool m_olClipped;
    wxOL_Region *m_olRegion;    // installed in the layer while drawing
    wxOL_Region *m_olOldRegion;
    bool m_olDirect;             // true colour: FillPixelArray
    wxOLPens *m_olPens;

    wxDECLARE_ABSTRACT_CLASS(wxOLDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxOLDCImpl);
};

#endif // _WX_OPENLAYOUT_DC_H_
