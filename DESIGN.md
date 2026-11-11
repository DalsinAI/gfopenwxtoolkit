# GFOpenWX Toolkit design

GFOpenWX Toolkit (gfopenwx) is a wxWidgets port for AmigaOS 3 (68040, soft float), called "openlayout" (`__WXOPENLAYOUT__`). It works the way wxGTK and wxQt do: wxWidgets 3.2 is fetched at build time for wxBase and the common GUI code, and this repository holds the port itself.

Dale chose this route on 4 Nov 2026, over writing all of wx ourselves. It is meant to make wx apps easy to port, with Audacity 3.7.9 as the first.

## Pieces

| Piece | Where | What |
|---|---|---|
| wxWidgets 3.2.12 | fetched (`wx.lock`) | wxBase, common GUI code, generic widgets (grid, list control, tree, data view, dialogs, status bar) |
| `patches/` | here | the few changes wx's own files need (configure knows m68k-amigaos) |
| `scripts/add-toolkit.py` | here | wires `__WXOPENLAYOUT__` into wx's dispatch headers, next to each `__WXQT__` branch, for the headers this port has |
| `include/wx/openlayout/`, `src/openlayout/` | here | the port: app, event loop, windows, DC, fonts, bitmaps, controls, menus, clipboard |
| `src/amiga/` | here | the C library calls wxBase needs that libnix lacks (wide-character numbers and scanf, statvfs and the like) |
| GFOpenLayout UI | fetched (gfopenlayout) | draws and runs every window |

## How a window works

- **One GFOpenLayout UI per top-level window.** A wxFrame or wxDialog is an Intuition window holding one `olui` (sharing the first window's stylesheet through `olui_new_sharing`) and one `olui_amiga`, drawn in the theme GFOpenPrefs Look chose.
- **Every wxWindow is an element in that UI.** Each one is a positioned `div` (`position:absolute`, placed with `olui_set_place`, sized with `olui_set_style`) inside its parent's element. wx's sizers do the layout and GFOpenLayout draws it, so z-order, clipping, hit tests and pop-ups are all olui's.
- **Native controls are olui widgets.** wxButton is a `button`, wxCheckBox a `checkbox`, wxTextCtrl an `input` or `textarea`, wxChoice a `select`, wxSlider a `slider`, wxSpinCtrl a `spinner`, wxListBox a `list`, wxListCtrl (report and virtual) a `vlist` with columns, wxTreeCtrl a `tree`, wxNotebook `tabs`, wxGauge a `progress`, and popup menus olui context menus. olui's events (click, change, select, activate, expand and collapse, focus) become wx's command events.
- **Custom-drawn windows** (a wxWindow or wxPanel that paints in EVT_PAINT, such as Audacity's track panel, rulers and meters) hold an `img` element filling their box. olui calls the program's picture drawer for it, into the window's buffer cut to what is being drawn. The port sends the window a wxPaintEvent there, with a wxPaintDC on that RastPort, so wx drawing lands in olui's double buffer in the right layer.
- **Mouse and keys** go to olui first. Where olui doesn't take them, `olui_hit` finds the element under the pointer, and the port sends that wxWindow wxMouseEvents (with capture), or sends wxKeyEvents to the window with the focus.
- **Menu bars** are Intuition menus (GadTools), the Amiga way: right button, screen title bar, Amiga-key shortcuts from wx's accelerators.
- **Timers and idle**: the event loop `Wait()`s on the windows' ports, a timer.device request for the next wx timer, and the wake signal threads use (`wxWakeUpIdle`). wx idle events run when the queue is empty.
- **Sockets**: wx's sockets are libnix's (`-lsocket`, bsdsocket.library opened for the main task). When wx watches any, the loop waits with `WaitSelect` on their stack descriptors and the same signal mask, and calls their handlers after (`src/openlayout/sockets.cpp`). Sockets are used from the main task only: bsdsocket.library is opened per task.
- **The system's own libraries, not compiled-in copies.** zlib is z.library (GFOpenZlib): `libwxzlib` holds only `src/amiga/wxzlib_zlibrary.c`, which calls the library through its LVOs (opened on first use), and `src/amiga/wxzlib_gzfile.c`, zlib's gz* file calls (which z.library lacks) on those calls and libnix's stdio. wxImage reads PNG and JPEG through datatypes.library (`src/openlayout/dtimage.cpp`: the picture datatypes give ARGB pixels through `PDTM_READPIXELARRAY`), and writes PNG through gfopenpng.datatype's `OIM_ENCODEPNG`; JPEG can't be written, as no datatype writes it. No libpng or libjpeg is built. Expat, regex, NanoSVG and Scintilla stay compiled in, since there's no system library for them.

- **ARexx**: `wxARexxPort` (`wx/openlayout/arexx.h`, `src/openlayout/arexx.cpp`) gives a program an ARexx port on arexx.class (OS 3.2's or ours). The program adds commands (name, ReadArgs template, handler); QUIT, ACTIVATE, DEACTIVATE, VERSION, HELP and FAULT are there from the start. The loop waits for the port's signal (a watch list in `app.cpp`) and runs commands on the main thread; a command that opens a window of its own does not run the class again inside itself. QUIT that comes while a dialog's loop runs (a file requester is up) ends those dialogs as Cancel and closes the window once their loops are over: closing it inside the loop deleted the dialog under its ShowModal (Guru 81000005 at exit). A command that answers only once its work is done waits with `WaitFor(cmd, ready, timeout)`, which also runs the queued events (CallAfter, QueueEvent) and answers RC 10 on timeout, RC 20 when the program is closing. `ac_Func` is a small thunk that sets A4, so it works with wx in the shared module. Checked with `tests/gui/arexxtest` and `tests/amiga/arexxtest.rexx`.
- **OpenGL**: `wxGLCanvas` and `wxGLContext` (`wx/openlayout/glcanvas.h`, `src/openlayout-gl/glcanvas.cpp`) are wx's GL library, `libwx_openlayoutu_gl` (`wx-config --libs gl`, which adds `-lGL`), on GFOpenGPU's GL module through GLA: virgl on AmiCradle's GPU, softpipe on the 68k elsewhere. A context is a GLA context; a canvas has a GLA buffer of its size. The canvas's element is a plain box that olui copies its buffer around (`olui_amiga_own`, four to a window); each time the window is drawn there the canvas gets its paint event, and `SwapBuffers` writes the frame straight into the Intuition window at the box. A pop-up olui draws over the canvas is hidden under it. It is built when the stove carries the GFOpenGPU developer kit (`GL/gl.h`, `gla/`, `libGL.a`) or `GLKIT` names one; soft float like the rest (GL takes its numbers on the stack either way). Checked with `tests/gl/glspin`.

## The DC

wxDC is drawn through graphics.library into the RastPort it is given (olui's buffer for paint, an off-screen BitMap for wxMemoryDC). True-colour pens and brushes use cybergraphics' `FillPixelArray` and `WritePixelArray`; lines, polygons and arcs use graphics.library's `AreaDraw`/`Draw` with the pen's RGB obtained through `ObtainBestPen` on a palette screen. Text goes through diskfont.library (GFOpenFont).

## What GFOpenLayout needs adding

- `olui_repaint(ui, el)`: marks an element to be drawn again, for wxWindow::Refresh on a custom-drawn window.
- A grid and a splitter, for wxGrid and wxSplitterWindow, unless wx's generic ones (drawn through the wxDC) are good enough; they are used first.

## Order

1. wxBase builds and passes `tests/base` on AmigaOS.
2. The GUI library builds: app, event loop, top-level windows, the DC, fonts and bitmaps, then the controls.
3. wx's `minimal` sample runs; then `widgets` and `dialogs`.
4. Audacity.
