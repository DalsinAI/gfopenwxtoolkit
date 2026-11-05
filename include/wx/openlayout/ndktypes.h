/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/ndktypes.h
// Purpose:     NDK types in the port's public headers, without their names
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_NDKTYPES_H_
#define _WX_OPENLAYOUT_NDKTYPES_H_

// The NDK's Window, Menu, Region and BitMap are plain global names. Declaring
// them in public wx headers clashes with applications' own names (Audacity's
// MenuRegistry::Menu, for one), so applications only see opaque stand-ins.
// The port's own sources (WXBUILDING) include the NDK and get the real types.
// Both are pointers only, so class layouts are the same either way.
#ifdef WXBUILDING
    struct Window; struct Menu; struct Region; struct BitMap;
    #define wxOL_Window struct Window
    #define wxOL_Menu   struct Menu
    #define wxOL_Region struct Region
    #define wxOL_BitMap struct BitMap
#else
    struct wxOLNDKWindow; struct wxOLNDKMenu; struct wxOLNDKRegion; struct wxOLNDKBitMap;
    #define wxOL_Window struct wxOLNDKWindow
    #define wxOL_Menu   struct wxOLNDKMenu
    #define wxOL_Region struct wxOLNDKRegion
    #define wxOL_BitMap struct wxOLNDKBitMap
#endif

#endif // _WX_OPENLAYOUT_NDKTYPES_H_
