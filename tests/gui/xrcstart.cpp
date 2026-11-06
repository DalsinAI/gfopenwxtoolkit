/* xrcstart: a dialog from XRC (read by OXP through wxXmlDocument), shown,
 * its controls found by name, then the program ends at the first idle
 * time: the start of a small wx GUI program, end to end.
 * Prints "xrcstart: N failed". Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/xrc/xmlres.h>
#include <wx/xml/xml.h>
#include <wx/mstream.h>
#include <stdio.h>

static const char xrc[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<resource xmlns=\"http://www.wxwidgets.org/wxxrc\" version=\"2.5.3.0\">\n"
    " <object class=\"wxFrame\" name=\"main\">\n"
    "  <title>xrcstart</title>\n"
    "  <object class=\"wxBoxSizer\"><orient>wxVERTICAL</orient>\n"
    "   <object class=\"sizeritem\"><object class=\"wxStaticText\" name=\"label\"><label>Caf\xc3\xa9 &amp; co</label></object></object>\n"
    "   <object class=\"sizeritem\"><object class=\"wxTextCtrl\" name=\"entry\"><value>hello</value></object></object>\n"
    "   <object class=\"sizeritem\"><object class=\"wxButton\" name=\"ok\"><label>OK</label></object></object>\n"
    "  </object>\n"
    " </object>\n"
    "</resource>\n";

class App : public wxApp
{
    int failed = 0;
    void Check(bool ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); failed += !ok; }
public:
    bool OnInit() wxOVERRIDE
    {
        wxXmlResource::Get()->InitAllHandlers();
        wxMemoryInputStream in(xrc, sizeof xrc - 1);
        wxXmlDocument *doc = new wxXmlDocument(in);
        Check(doc->IsOk() && wxXmlResource::Get()->LoadDocument(doc, "xrcstart"), "XRC loaded");
        wxFrame *f = wxXmlResource::Get()->LoadFrame(NULL, "main");
        Check(f != NULL, "frame from XRC");
        if (!f) { printf("xrcstart: %d failed\n", failed); return false; }
        wxStaticText *l = XRCCTRL(*f, "label", wxStaticText);
        wxTextCtrl *e = XRCCTRL(*f, "entry", wxTextCtrl);
        Check(l && l->GetLabel() == wxString::FromUTF8("Caf\xc3\xa9 & co"), "label text");
        Check(e && e->GetValue() == "hello", "text control value");
        Check(XRCCTRL(*f, "ok", wxButton) != NULL, "button");
        f->Show();
        CallAfter([this, f] { printf("xrcstart: %d failed\n", failed); f->Destroy(); });
        return true;
    }
    int OnExit() wxOVERRIDE { return failed ? 10 : 0; }
};

wxIMPLEMENT_APP(App);
