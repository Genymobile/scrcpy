#ifndef SC_PROCESS_INTR_H
#define SC_PROCESS_INTR_H

#include "common.h"

#include "intr.h"
#include "process.h"

bool
sc_process_check_success_intr(struct sc_intr *intr, sc_pid pid,
                              const char *name);

#endif
