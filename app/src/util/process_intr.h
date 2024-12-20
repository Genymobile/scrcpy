#ifndef SC_PROCESS_INTR_H
#define SC_PROCESS_INTR_H

#include "common.h"

#include "util/intr.h"
#include "util/process.h"

ssize_t
sc_pipe_read_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe, char *data,
                  size_t len);

ssize_t
sc_pipe_read_all_intr(struct sc_intr *intr, sc_pid pid, sc_pipe pipe,
                      char *data, size_t len);

#endif
