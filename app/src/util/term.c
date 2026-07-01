#include "term.h"

#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
# include <windows.h>
# include <io.h>
# include "util/log.h"
# include "util/str.h"
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

static inline bool
sc_term_stdout_isatty(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}

// Return true if VT/ANSI escape sequences written to stdout are processed by
// the terminal (rather than printed verbatim).
//
// On Linux, a tty always processes escape sequences, so this is equivalent to
// sc_term_stdout_isatty(). On Windows, VT processing must be explicitly enabled
// (and is not supported by legacy console hosts), otherwise the escape
// sequences would be printed verbatim.
static bool
sc_term_stdout_supports_vt(void) {
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || !handle) {
        return false;
    }

    DWORD mode;
    if (!GetConsoleMode(handle, &mode)) {
        // stdout is not a console (redirected to a file or a pipe)
        return false;
    }

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) {
        return true;
    }

    // Try to enable VT processing (supported since Windows 10). On legacy
    // console hosts, this fails, so no escape sequence must be emitted.
    return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    return isatty(STDOUT_FILENO);
#endif
}

void
sc_term_set_title(const char *title) {
    if (!sc_term_stdout_isatty()) {
        return;
    }

#ifdef _WIN32
    wchar_t *wtitle = sc_str_to_wchars(title);
    if (wtitle) {
        SetConsoleTitleW(wtitle);
        free(wtitle);
    } else {
        LOGW("Could not convert title to wchar");
        SetConsoleTitleA(title);
    }
#else
    // Emit an OSC 0 escape sequence recognized by most terminals
    printf("\033]0;%s\007", title);
    fflush(stdout);
#endif
}

void
sc_term_save_title(void) {
    if (sc_term_stdout_supports_vt()) {
        printf("\033[22;2t");
    }
}

void
sc_term_restore_title(void) {
    if (sc_term_stdout_supports_vt()) {
        printf("\033[23;2t");
        fflush(stdout);
    }
}
