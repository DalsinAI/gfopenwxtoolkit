/////////////////////////////////////////////////////////////////////////////
// Name:        wx/openlayout/app.h
// Purpose:     wxApp for the openlayout port (AmigaOS, GFOpenLayout UI)
// Copyright:   (c) 2026 AmiCradle Team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef _WX_OPENLAYOUT_APP_H_
#define _WX_OPENLAYOUT_APP_H_

class WXDLLIMPEXP_CORE wxApp : public wxAppBase
{
public:
    wxApp();
    virtual ~wxApp();

    // opens the libraries the port draws with and locks the screen
    virtual bool Initialize(int& argc, wxChar **argv) wxOVERRIDE;
    virtual void CleanUp() wxOVERRIDE;

    virtual void WakeUpIdle() wxOVERRIDE;

private:
    wxDECLARE_DYNAMIC_CLASS(wxApp);
};

#endif // _WX_OPENLAYOUT_APP_H_
