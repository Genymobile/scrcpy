#ifndef SC_HID_EVENT_SERIALIZER_H
#define SC_HID_EVENT_SERIALIZER_H

#include "common.h"

#include <stdbool.h>

#include "hid_event.h"
#include "util/vecdeque.h"

// hid_event_parser: convert from bytes to sc_hid_event

enum sc_hid_event_parser_status {
    SC_HID_EVENT_PARSER_STATE_GOOD,
    SC_HID_EVENT_PARSER_STATE_CORRUPT,
};
struct sc_hid_event_ptr_queue SC_VECDEQUE(struct sc_hid_event*);

struct sc_hid_event_parser {
    const char *source_name; // Used in log messages, e.g. filename.
    int line; // Line that is being parsed.

    char *data; // A zero-terminated string with string length data_len.
    size_t data_offset; // Offset in data where we should start parsing.
    size_t data_len; // Length of string, excluding NUL byte.
    size_t data_buffer_size; // Size of |data|, including NUL and unused bytes.

    struct sc_hid_event_ptr_queue parsed_events;
    enum sc_hid_event_parser_status parser_status;

    // Adding input and parsing input are separate, and receiving the input
    // without parsing could already fail (e.g. due to OOM). This failure is
    // stored separately, and eventually propagates when the parser starts.
    bool input_data_was_corrupt;
};

void
sc_hid_event_parser_init(struct sc_hid_event_parser *hep,
                         const char *source_name);

void
sc_hid_event_parser_destroy(struct sc_hid_event_parser *hep);

// Appends data without taking ownership of |data|. |data| contains |data_len|
// bytes.
bool
sc_hid_event_parser_append_data(struct sc_hid_event_parser *hep,
                                const char *data, size_t data_len);

// Parse all data that has been appended so far. In multi-threaded situations,
// this must be mutually exclusive with all other sc_hid_event_parser* methods.
void
sc_hid_event_parser_parse_all_data(struct sc_hid_event_parser *hep);

// Check if a parsed event is available. This only includes events that were
// parsed until the most recent call to sc_hid_event_parser_parse_all_data.
bool
sc_hid_event_parser_has_next(struct sc_hid_event_parser *hep);

// Retrieve the next event, if available. This only includes events that were
// parsed until the most recent call to sc_hid_event_parser_parse_all_data.
struct sc_hid_event*
sc_hid_event_parser_get_next(struct sc_hid_event_parser *hep);

bool
sc_hid_event_parser_has_error(struct sc_hid_event_parser *hep);

bool
sc_hid_event_parser_has_unparsed_data(struct sc_hid_event_parser *hep);

// hid_event_serializer: convert from sc_hid_event to bytes

struct sc_hid_event_serializer {
    char *data;
    size_t data_len; // Length of string, excluding NUL byte.
    size_t data_buffer_size; // Size of |data|, including NUL and unused bytes.
};

void
sc_hid_event_serializer_init(struct sc_hid_event_serializer *hes);

void
sc_hid_event_serializer_destroy(struct sc_hid_event_serializer *hes);

// Appends serialization of |event| to |hes->data|.
bool
sc_hid_event_serializer_update(struct sc_hid_event_serializer *hes,
                               struct sc_hid_event *event);

// To minimize allocations, callers can directly copy |hes->data_len| bytes to
// their destination from |hes->data|. After that, mark the data as read to
// allow the space to be freed for further data.
void
sc_hid_event_serializer_mark_as_read(struct sc_hid_event_serializer *hes);
#endif
