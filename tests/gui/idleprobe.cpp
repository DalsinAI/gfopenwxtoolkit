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
extern "C" unsigned long __wxtk_app[] __attribute__((weak));

static long g_idleType;

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
            g_idleType = ev ? (long)ev->GetEventType() : -1L;
            printf("idleprobe: the module's idle event type %ld, next new type %ld\n",
                   ev ? (long)ev->GetEventType() : -1L, (long)wxNewEventType());
            delete o;
            if ( __wxtk_app && ev )
            {
                const unsigned char *copy = (const unsigned char *)__wxtk_app[18];
                const unsigned long *mh = (const unsigned long *)__wxtk_app[19];
                printf("idleprobe: copy %p, a4_init %p, data_size %lu, program's offset %lx\n", (const void *)copy,
                       (void *)mh[5], mh[6], (unsigned long)((const unsigned char *)&wxEVT_IDLE - copy));
                fflush(stdout);
                int n = 0;
                for ( unsigned long k = 0; k + 4 <= mh[6] && n < 8; k += 2 )
                    if ( *(const long *)(copy + k) == g_idleType )
                        { printf("idleprobe: the module's idle type found at offset %lx\n", k); n++; }
                fflush(stdout);
            }
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
