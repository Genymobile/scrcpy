#include "term.h"

#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
# include <io.h>
# include <windows.h>
#else
# include <unistd.h>
# include <sys/ioctl.h>
#endif

bool
sc_term_get_size(unsigned *rows, unsigned *cols) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    bool ok =
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    if (!ok) {
        return false;
    }

    if (rows) {
        assert(csbi.srWindow.Bottom >= csbi.srWindow.Top);
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    if (cols) {
        assert(csbi.srWindow.Right >= csbi.srWindow.Left);
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }

    return true;
#else
    struct winsize ws;
    int r = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    if (r == -1) {
        return false;
    }

    if (rows) {
        *rows = ws.ws_row;
    }

    if (cols) {
        *cols = ws.ws_col;
    }

    return true;
#endif
}

void
sc_term_set_title(const char *title) {
#ifdef _WIN32
    // Fallback for legacy Windows console hosts (cmd.exe / conhost) that do
    // not process VT escape sequences.
    SetConsoleTitleA(title);

    if (!_isatty(_fileno(stdout))) {
        return;
    }
#else
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
#endif
    // OSC 0 sets both the icon name and the window/tab title; recognized by
    // Windows Terminal, macOS Terminal, xterm, and most modern emulators.
    printf("\033]0;%s\007", title);
    fflush(stdout);
}
