#include "term.h"

#include <assert.h>

#ifdef _WIN32
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
