/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/defs.h
// Purpose:     the types the port's handles have
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_DEFS_H_
#define _WX_OPENLAYOUT_DEFS_H_

// A window handle is the wxWindow itself: GFOpenLayout elements aren't
// windows of their own.
typedef void* WXWidget;
typedef void* WXWindow;

#endif // _WX_OPENLAYOUT_DEFS_H_
