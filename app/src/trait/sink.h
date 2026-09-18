#ifndef SC_SINK_H
#define SC_SINK_H

#include "common.h"

enum sc_sink_result {
    /* The item has been accepted */
    SC_SINK_OK,

    /* An error occurred */
    SC_SINK_KO,

    /* The sink is stopped and will not accept any further item (this is not an
     * error) */
    SC_SINK_STOPPED,
};

#endif
