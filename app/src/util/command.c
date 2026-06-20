#include "command.h"

#include <stdlib.h>

#include "util/strbuf.h"

char *
sc_command_serialize_windows(const char *const argv[]) {
    // <https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments>
    // <https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw>

    struct sc_strbuf buf;
    bool ok = sc_strbuf_init(&buf, 1024);
    if (!ok) {
        return NULL;
    }

#define BUF_PUSH(C) \
    do { \
        if (!sc_strbuf_append_char(&buf, C)) { \
            goto end; \
        } \
    } while (0)

    while (*argv) {
        const char *arg = *argv;

        BUF_PUSH('"');

        // <https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments>
        //
        // """
        // If an even number of backslashes is followed by a double quote mark,
        // then one backslash (\) is placed in the argv array for every pair of
        // backslashes (\\), and the double quote mark (") is interpreted as a
        // string delimiter.
        //
        // If an odd number of backslashes is followed by a double quote mark,
        // then one backslash (\) is placed in the argv array for every pair of
        // backslashes (\\). The double quote mark is interpreted as an escape
        // sequence by the remaining backslash, causing a literal double quote
        // mark (") to be placed in argv.
        // """
        //
        // To produce correct escaping according to what the parser will do, we
        // must count the number of successive backslashes.
        unsigned backslashes = 0;

        for (const char *c = arg; *c; c++) {
            switch (*c) {
                case '"':
                    while (backslashes) {
                        // Double all backslashes before a quote
                        BUF_PUSH('\\');
                        BUF_PUSH('\\');
                        --backslashes;
                    }
                    BUF_PUSH('\\');
                    BUF_PUSH('"');
                    backslashes = 0;
                    break;
                case '\\':
                    ++backslashes;
                    break;
                default:
                    while (backslashes) {
                        // Put all backslashes as literals
                        BUF_PUSH('\\');
                        --backslashes;
                    }
                    BUF_PUSH(*c);
                    break;
            }
        }

        while (backslashes) {
            // Double all backslashes before a quote
            BUF_PUSH('\\');
            BUF_PUSH('\\');
            --backslashes;
        }

        BUF_PUSH('"');

        ++argv;

        // Argument separator
        if (*argv) {
            BUF_PUSH(' ');
        }
    }

    return buf.s;

end:
    free(buf.s);
    return NULL;
}
