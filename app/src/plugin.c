#include "plugin.h"
#include "scrcpy.h"
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#ifdef _WIN32
// not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>
#endif

#ifdef _WIN32

#define WM_PLUGIN_BASE WM_USER 
void on_window_message(void *userdata, void *hWnd, unsigned int message, Uint64 wParam, int64_t lParam){
    (void)(userdata);
    (void)(hWnd);
    if (message >= WM_PLUGIN_BASE ){
        message -= WM_PLUGIN_BASE;
        switch (message)
        {
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
            {
                SDL_Event sdlevent;
                sdlevent.type = message;
                sdlevent.tfinger.fingerId = 0;
                sdlevent.tfinger.pressure = 0.8;
                sdlevent.tfinger.x  = (double)(wParam) / (1000*1000);
                sdlevent.tfinger.y  = (double)(lParam) / (1000*1000);
                SDL_PushEvent(&sdlevent);
            }
            break;
        default:
            break;
        }
        
    }
}
#endif


void plugin_init(){
#ifdef _WIN32
    SDL_SetWindowsMessageHook(on_window_message,NULL);
#endif
}
