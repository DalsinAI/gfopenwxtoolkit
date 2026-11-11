# GFOpenWX Toolkit

wxWidgets for AmigaOS 3, on GFOpenLayout. A wx program builds for the Amiga the way it builds for GTK or Qt: its windows, controls and menus become GFOpenLayout UI and Intuition menus, drawn in the theme GFOpenPrefs Look chose.

The port is called "openlayout" (`__WXOPENLAYOUT__`). wxWidgets 3.2.12 is fetched at build time for wxBase and the common GUI code, and this repository holds the port itself. Audacity 3.7.9 (gfamigaaudacity) is the first program built on it.

- 68040, soft float: one binary with or without an FPU.
- Native controls are GFOpenLayout widgets; custom-drawn windows get their wxPaintEvent in GFOpenLayout's double buffer.
- wxNet: wxSocket on libnix's sockets (bsdsocket.library); the GUI loop waits on sockets and windows together (WaitSelect).
- wxWebRequest: wx's libcurl backend on DalsinAI/gfopencurl (HTTP and HTTPS, TLS through gfopentls.library, HTTP/2), built at the commit deps.lock gives (`scripts/build-curl.sh`); `wx-config --libs net` links it. Its sockets are watched by the GUI loop too, so a request runs while windows answer. At run time HTTPS needs `LIBS:gfopentls.library`; give a program 128 KB of stack (a TLS handshake takes 16 KB deep in curl). `tests/gui/webtest` fetches addresses with it.
- Window scroll bars, the mouse wheel (OS 3.2's and NewMouse's), menus from wx's accelerators, wxSound on AHI, fonts through diskfont.library.

## Building

```
STOVE=~/AmiCradle/stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh
```

`out/` gets the wxBase tests (`basetest`, `wctest`, `sockettest`), wx's `minimal` sample and `sdk/`: the headers, libraries and `bin/wx-config` a wx program builds with:

```
m68k-amigaos-g++ $(sdk/bin/wx-config --cxxflags) -c app.cpp
m68k-amigaos-g++ -noixemul -m68040 -specs=scripts/m68k-soft-m68040.specs -o app app.o $(sdk/bin/wx-config --libs)
```

wx.lock pins wxWidgets; deps.lock pins GFOpenLayout UI, GFOpenGadTools' theme reader, regex.library, OXP and gfopencurl (with curl's and nghttp2's upstream tarballs).

## openwxtoolkit.library (shared)

wx programs can share one wxWidgets: `openwxtoolkit.library` loads `LIBS:openwxtoolkit/wx.module` (wx built `-fbaserel32`) once, and each program gets its own copy of the module's data. A program links against it with `WXTK_SHARED=1 wx-config --libs` and is about 0.8 MB instead of 2 to 7 MB.

```
STOVE=~/AmiCradle/stoves/os32-gcc16-open6 OUT=out sh library/build-amiga.sh
```

builds wx `-fbaserel32`, the library, its module, the tests linked shared and the package `GFOpenWXToolkit-1.0-rc2.lha` (Amiga Installer script `package/Install-GFOpenWXToolkit`). It needs regex.library and z.library at run time. `src/library/` holds the library, the glue each program links and the module's runtime; `scripts/wxtk-gen.py`, `scripts/build-library.sh` and `scripts/wxtk-absown.py` make the module.

## Layout

| Where | What |
|---|---|
| `src/openlayout/`, `include/wx/openlayout/` | the port |
| `src/amiga/` | the C library calls wxBase needs that libnix lacks |
| `patches/` | the few changes wx's own files need |
| `scripts/` | the build, the toolkit wiring (`add-toolkit.py`), `wx-config` |
| `config/` | the GUI features and sources built |
| `tests/base/` | wxBase tests |

See DESIGN.md for how a window works.

## Licence

The port's files are under the wxWindows Library Licence (LICENSE-wxWindows.txt), as wxWidgets is.

Designed, tested and delivered by The AmiCradle Team.
Copyright (c) 2026 AmiCradle Team
