GFOpenWX Toolkit @VERSION@
openwxtoolkit.library: wxWidgets for AmigaOS 3 on GFOpenLayout, shared.
Its code is in memory once (LIBS:openwxtoolkit/wx.module); each wx
program gets its own copy of the module's data. AmiCradle Team.

Install:   double-click Install-GFOpenWXToolkit (beside this drawer), or
           Copy Libs/openwxtoolkit.library LIBS:
           MakeDir LIBS:openwxtoolkit
           Copy Libs/openwxtoolkit/wx.module LIBS:openwxtoolkit/
Needs:     regex.library (GFOpenRegex), z.library (GFOpenZlib),
           datatypes for PNG and JPEG, AmigaOS 3.2.
Tests:     CD GFOpenWXToolkit/Tests, then basetest (expects
           "basetest: 28 passed, 0 failed"), xrcstart ("xrcstart: 0 failed"),
           wctest, sockettest, minimal (a window).
Programs:  build with The Open SDK: WXTK_SHARED=1 wx-config --libs links
           a program against the library instead of a static wx.

Built from commit @SHA@, 68040, soft float.
Designed, tested and delivered by The AmiCradle Team.
