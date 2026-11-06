/* idleprobe (temporary): what the main loop's idle time does, line by line,
 * flushed as it goes. Copyright (c) 2026 AmiCradle Team */
#include <wx/wx.h>
#include <wx/thread.h>
#include <stdio.h>

static void Say(const char *s, long v = 0)
{
    printf("idleprobe: %s %ld\n", s, v);
    fflush(stdout);
}

extern "C" void *__wxtk_a4 __attribute__((weak));

class App : public wxApp
{
    int idles = 0;
public:
    bool OnInit() wxOVERRIDE
    {
        Say("main thread", (long)wxThread::IsMain());
        printf("idleprobe: a4 %p\n", &__wxtk_a4 ? __wxtk_a4 : NULL);
        printf("idleprobe: wxEVT_IDLE at %p = %ld, wxEVT_ASYNC_METHOD_CALL at %p = %ld, wxEVT_BUTTON = %ld\n",
               (const void *)&wxEVT_IDLE, (long)(wxEventType)wxEVT_IDLE,
               (const void *)&wxEVT_ASYNC_METHOD_CALL, (long)(wxEventType)wxEVT_ASYNC_METHOD_CALL, (long)(wxEventType)wxEVT_BUTTON);
        {
            wxObject *o = wxCreateDynamicObject("wxIdleEvent");
            wxEvent *ev = wxDynamicCast(o, wxEvent);
            printf("idleprobe: the module's idle event type %ld, next new type %ld\n",
                   ev ? (long)ev->GetEventType() : -1L, (long)wxNewEventType());
            delete o;
        }
        printf("idleprobe: wxTheApp %p (this %p)\n", (void *)wxTheApp, (void *)this);
        fflush(stdout);
        wxFrame *f = new wxFrame(NULL, wxID_ANY, "idleprobe");
        f->Show();
        Bind(wxEVT_IDLE, [this, f](wxIdleEvent& e) {
            Say("idle", ++idles);
            if ( idles == 3 )
            {
                Say("pending before CallAfter", (long)HasPendingEvents());
                CallAfter([] { Say("CallAfter ran"); });
                Say("pending after CallAfter", (long)HasPendingEvents());
            }
            if ( idles == 6 )
                f->Destroy();
            if ( idles < 6 )
                e.RequestMore();
        });
        Say("bound, pending processing", (long)wxTheApp->IsScheduledForDestruction(f));
        return true;
    }
    int OnExit() wxOVERRIDE { Say("exit", idles); return 0; }
};

wxIMPLEMENT_APP(App);
