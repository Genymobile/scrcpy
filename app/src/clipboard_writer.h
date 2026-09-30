#ifndef SC_CLIPBOARD_WRITER_H
#define SC_CLIPBOARD_WRITER_H

#include <stdbool.h>
#include <stddef.h>

// Take ownership of data and publish it as PNG, even on failure.
bool
sc_clipboard_write_png(void *data, size_t size);

#endif
