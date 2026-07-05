#ifndef SCRCPY_WIN_QUICK_TOGGLE_H
#define SCRCPY_WIN_QUICK_TOGGLE_H

#include <stdbool.h>

#include <SDL3/SDL.h>

// Windows-only global hotkey + scrcpy window toggle.
//
// This module does not create a second SDL window.
// It only hides/shows the existing SDL window and sets it as a tool window
// (no taskbar entry).

#ifdef _WIN32

bool
sc_win_quick_toggle_init(SDL_Window *window);

void
sc_win_quick_toggle_destroy(void);

// Internal: handle a Windows MSG (specifically WM_HOTKEY).
bool
sc_win_quick_toggle_handle_msg(const MSG *msg);

#endif // _WIN32

#endif // SCRCPY_WIN_QUICK_TOGGLE_H


