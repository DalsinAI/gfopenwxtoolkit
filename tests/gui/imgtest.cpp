/* wxImage's PNG and JPEG through datatypes: makes a picture with alpha,
 * saves it as PNG (gfopenpng.datatype's encoder), loads it back and checks
 * every pixel; then loads any PNG or JPEG named on the command line and
 * prints its size. Prints "imgtest: N failed".
 * Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <stdio.h>

class App : public wxApp
{
public:
    bool OnInit() wxOVERRIDE
    {
        int failed = 0;
        wxInitAllImageHandlers();
        const int w = 37, h = 21;
        wxImage img(w, h, false);
        img.SetAlpha();
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                img.SetRGB(x, y, x * 7, y * 12, (x + y) * 3);
                img.SetAlpha(x, y, (x * 255) / (w - 1));
            }
        wxMemoryOutputStream out;
        if (!img.SaveFile(out, wxBITMAP_TYPE_PNG)) {
            printf("imgtest: PNG save failed\n");
            failed++;
        } else {
            printf("imgtest: PNG saved, %lu bytes\n", (unsigned long)out.GetSize());
            wxMemoryInputStream in(out);
            wxImage back;
            if (!back.LoadFile(in, wxBITMAP_TYPE_ANY)) {
                printf("imgtest: PNG load failed\n");
                failed++;
            } else if (back.GetWidth() != w || back.GetHeight() != h || !back.HasAlpha()) {
                printf("imgtest: PNG back as %dx%d alpha %d\n", back.GetWidth(), back.GetHeight(), back.HasAlpha());
                failed++;
            } else {
                int bad = 0;
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        if (back.GetRed(x, y) != img.GetRed(x, y) || back.GetGreen(x, y) != img.GetGreen(x, y)
                            || back.GetBlue(x, y) != img.GetBlue(x, y) || back.GetAlpha(x, y) != img.GetAlpha(x, y))
                            bad++;
                printf("imgtest: PNG round trip, %d pixels differ\n", bad);
                failed += bad != 0;
            }
        }
        for (int i = 1; i < argc; i++) {
            wxImage f;
            if (f.LoadFile(argv[i]))
                printf("imgtest: %s %dx%d alpha %d\n", (const char *)argv[i].mb_str(), f.GetWidth(), f.GetHeight(), f.HasAlpha());
            else {
                printf("imgtest: %s did not load\n", (const char *)argv[i].mb_str());
                failed++;
            }
        }
        printf("imgtest: %d failed\n", failed);
        return false;
    }
};

wxIMPLEMENT_APP(App);
