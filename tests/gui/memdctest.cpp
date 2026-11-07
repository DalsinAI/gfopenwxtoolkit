// memdctest: draws into a bitmap through a wxMemoryDC, deselects it, and
// reads the bitmap back: what was drawn must still be there (DeleteLayer
// used to backfill the whole bitmap with pen 0 when the DC let it go).
// Writes RAM:memdctest.log and exits.
// Copyright (c) 2026 AmiCradle Team
#include <wx/wx.h>
#include <wx/dcmemory.h>
#include <stdio.h>

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        FILE *f = fopen("RAM:memdctest.log", "w");
        wxBitmap bmp(64, 32, 24);
        {
            wxMemoryDC dc(bmp);
            dc.SetBackground(*wxBLUE_BRUSH);
            dc.Clear();
            dc.SetBrush(*wxRED_BRUSH);
            dc.SetPen(*wxRED_PEN);
            dc.DrawRectangle(8, 8, 16, 16);
        }
        wxImage img = bmp.ConvertToImage();
        const int bx = img.GetBlue(2, 2), rr = img.GetRed(12, 12), rb = img.GetBlue(12, 12);
        const bool ok = bx > 200 && img.GetRed(2, 2) < 60 && rr > 200 && rb < 60;
        if ( f )
        {
            fprintf(f, "background %d,%d,%d rectangle %d,%d,%d\n", img.GetRed(2, 2), img.GetGreen(2, 2), bx, rr,
                    img.GetGreen(12, 12), rb);
            // select it again and deselect without drawing: still there
            {
                wxMemoryDC dc2;
                dc2.SelectObject(bmp);
                dc2.SelectObject(wxNullBitmap);
            }
            wxImage img2 = bmp.ConvertToImage();
            const bool ok2 = img2.GetRed(12, 12) > 200 && img2.GetBlue(2, 2) > 200;
            fprintf(f, "%s\n%s\n", ok ? "drawing kept" : "drawing LOST", ok2 ? "kept after reselect" : "LOST after reselect");
            fclose(f);
        }
        return false;
    }
};

wxIMPLEMENT_APP(App);
