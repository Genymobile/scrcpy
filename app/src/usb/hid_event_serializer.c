#include "util/log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "hid_event_serializer.h"
#include "util/tick.h"

#define LOG_REPLAY_PARSE_ERROR(fmt, ...) \
    LOGE("Failed to parse HID replay at line %d in %s: " fmt, \
            hep->line, hep->source_name, ## __VA_ARGS__)

static void
sc_hid_event_parser_parse_all_data_internal(struct sc_hid_event_parser *hep) {
    assert(hep->parser_status == SC_HID_EVENT_PARSER_STATE_GOOD);
    assert(hep->data[hep->data_len] == '\x00'); // Required for strchr/sscanf.

    char *data_iter = hep->data + hep->data_offset;

    assert(data_iter <= hep->data + hep->data_len);
    for (;;) {
        // Parse: [timestamp] [accessory_id] [buffer in hex, space-separated]\n
        char * const data_end_of_line = strchr(data_iter, '\n');
        if (!data_end_of_line) {
            // Cannot find end of line. Pause parser until we have more.
            break;
        }

        // Ignore empty lines and lines starting with #, to make manual editing
        // easier.
        if (data_iter == data_end_of_line || *data_iter == '#') {
            if (!strchr(data_iter, '\n')) {
                // Cannot find end of line. Pause parser until we have more.
                break;
            }
            data_iter = data_end_of_line + 1;
            ++hep->line;
            continue;
        }

        // Part 1 : timestamp
        sc_tick timestamp;
        if (sscanf(data_iter, "%" SCNtick " ", &timestamp) != 1) {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_REPLAY_PARSE_ERROR("Line must start with a numeric timestamp");
            break;
        }
        data_iter = strchr(data_iter, ' ');
        assert(data_iter); // We have already verified that there is a space.
        ++data_iter; // Eat space.
        if (*data_iter == '\x00') {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_REPLAY_PARSE_ERROR("Unexpected end of data");
            break;
        }

        // Part 2 : accessory_id
        uint16_t accessory_id;
        if (sscanf(data_iter, "%" SCNu16 " ", &accessory_id) != 1) {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_REPLAY_PARSE_ERROR("accessory_id must be a number");
            break;
        }
        data_iter = strchr(data_iter, ' ');
        assert(data_iter); // We have already verified that there is a space.

        // Part 3: buffer (repetition of space + 2 hex chars)
        assert(data_end_of_line > data_iter); // Only digits/space, no \n yet.
        size_t event_buffer_size = (data_end_of_line - data_iter) / 3;
        if (!event_buffer_size) {
            // Missing one or two characters after the space.
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_REPLAY_PARSE_ERROR("Unexpected %ld characters at end of line",
                    data_end_of_line - data_iter);
            break;
        }
        unsigned char *event_buffer = malloc(event_buffer_size);
        if (!event_buffer) {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_OOM();
            break;
        }
        for (size_t i = 0; i < event_buffer_size; ++i) {
            if (sscanf(data_iter, " %" SCNx8, &event_buffer[i]) != 1) {
                break;
            }
            data_iter += 3; // Skip space and 2 hexdigits.
        }
        if (data_iter != data_end_of_line) {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_REPLAY_PARSE_ERROR("Unexpected %ld characters at end of line",
                    data_end_of_line - data_iter);
            free(event_buffer);
            break;
        }
        assert(*data_iter == '\n'); // because data_iter == data_end_of_line
        ++data_iter; // Skip '\n'.
        ++hep->line;

        struct sc_hid_event *event = malloc(sizeof(struct sc_hid_event));
        if (!event) {
            hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
            LOG_OOM();
            free(event_buffer);
            break;
        }
        sc_hid_event_init(event, accessory_id, event_buffer,
                event_buffer_size);
        event->timestamp = timestamp;

        sc_vecdeque_push(&hep->parsed_events, event);
    }
    hep->data_offset = data_iter - hep->data;
    assert(hep->data_offset <= hep->data_len);
    assert(hep->data_offset < hep->data_buffer_size);
}

void
sc_hid_event_parser_init(struct sc_hid_event_parser *hep,
                         const char *source_name) {
    sc_vecdeque_init(&hep->parsed_events);
    hep->source_name = source_name;
    hep->line = 1;
    hep->data = NULL;
    hep->data_offset = 0;
    hep->data_len = 0;
    hep->data_buffer_size = 0;
    hep->parser_status = SC_HID_EVENT_PARSER_STATE_GOOD;
}

void
sc_hid_event_parser_destroy(struct sc_hid_event_parser *hep) {
    if (hep->data) {
        free(hep->data);
    }
    while (!sc_vecdeque_is_empty(&hep->parsed_events)) {
        struct sc_hid_event *event = sc_vecdeque_pop(&hep->parsed_events);
        assert(event);
        sc_hid_event_destroy(event);
        free(event);
    }
    sc_vecdeque_destroy(&hep->parsed_events);
}

bool
sc_hid_event_parser_append_data(struct sc_hid_event_parser *hep,
                                const char *data, size_t data_len) {
    if (hep->parser_status != SC_HID_EVENT_PARSER_STATE_GOOD) {
        // We are in the fatal SC_HID_EVENT_PARSER_STATE_CORRUPT state.
        return false;
    }
    if (!data_len) {
        return true;
    }
    assert(data);
    // Invariants:
    assert(hep->data_len >= hep->data_offset);
    // > not >= because data_buffer_size counts trailing NUL, data_len does not:
    assert(!hep->data || hep->data_buffer_size > hep->data_len);
    assert(!hep->data || hep->data[hep->data_len] == '\x00');

    size_t space_head = hep->data_offset;
    size_t space_middle = hep->data_len - hep->data_offset;
    // -1 to reserve space for NUL byte. Initially 0 because no data.
    size_t one_if_nul_tail = hep->data ? 1 : 0;
    size_t space_tail = hep->data_buffer_size - hep->data_len - one_if_nul_tail;
    size_t space_available = space_head + space_tail;
    // +1 to reserve space for NUL byte.
    size_t space_needed = space_middle + data_len + 1;
    if (space_needed < data_len) {
        // Integer overflowed. How did so much data fit in RAM...?
        hep->input_data_was_corrupt = true;
        LOG_OOM();
        return false;
    }
    if (data_len <= space_tail) {
        // Enough space at the end, simply insert at the end.
        assert(hep->data);
        memcpy(hep->data + hep->data_len, data, data_len);
        hep->data_len += data_len;
    } else if (space_needed <= space_available) {
        // There is space left at the start, move data and re-use buffer.
        assert(hep->data);
        memmove(hep->data, hep->data + hep->data_offset, space_middle);
        memcpy(hep->data + space_middle, data, data_len);
        hep->data_offset = 0;
        hep->data_len = space_middle + data_len;
    } else {
        // No space left, create a new buffer to replace the old one.
        char *new_data = malloc(space_needed);
        if (!new_data) {
            hep->input_data_was_corrupt = true;
            LOG_OOM();
            return false;
        }
        if (hep->data) {
            memcpy(new_data, hep->data + hep->data_offset, space_middle);
            free(hep->data);
        } else {
            assert(space_middle == 0);
        }
        memcpy(new_data + space_middle, data, data_len);
        hep->data = new_data;
        hep->data_offset = 0;
        hep->data_len = space_middle + data_len;
        hep->data_buffer_size = space_needed;
    }
    hep->data[hep->data_len] = '\x00';

    char *new_data_start = hep->data + hep->data_offset + space_middle;
    if (strlen(new_data_start) != data_len) {
        // The format does not expect NUL bytes. Disallow them to make sure that
        // we can use C string functions such as strchr without it tripping.
        hep->input_data_was_corrupt = true;
        LOGE("Unexpected NUL byte found in input data.");
        return false;
    }

    return true;
}

void sc_hid_event_parser_parse_all_data(struct sc_hid_event_parser *hep) {
    if (hep->input_data_was_corrupt) {
        hep->parser_status = SC_HID_EVENT_PARSER_STATE_CORRUPT;
    }
    if (hep->data && !sc_hid_event_parser_has_error(hep)) {
        sc_hid_event_parser_parse_all_data_internal(hep);
    }
}

bool
sc_hid_event_parser_has_next(struct sc_hid_event_parser *hep) {
    return !sc_vecdeque_is_empty(&hep->parsed_events);
}

struct sc_hid_event*
sc_hid_event_parser_get_next(struct sc_hid_event_parser *hep) {
    if (!sc_vecdeque_is_empty(&hep->parsed_events)) {
        return sc_vecdeque_pop(&hep->parsed_events);
    }
    return NULL;
}

bool
sc_hid_event_parser_has_error(struct sc_hid_event_parser *hep) {
    // Whether hep->status is SC_HID_EVENT_PARSER_STATE_CORRUPT.
    return hep->parser_status != SC_HID_EVENT_PARSER_STATE_GOOD;
}

bool
sc_hid_event_parser_has_unparsed_data(struct sc_hid_event_parser *hep) {
    return hep->data_offset != hep->data_len;
}

// sc_hid_event_serializer:

void
sc_hid_event_serializer_init(struct sc_hid_event_serializer *hes) {
    hes->data = NULL;
    hes->data_len = 0;
    hes->data_buffer_size = 0;
}

void
sc_hid_event_serializer_destroy(struct sc_hid_event_serializer *hes) {
    if (hes->data) {
        free(hes->data);
    }
}

bool
sc_hid_event_serializer_update(struct sc_hid_event_serializer *hes,
                               struct sc_hid_event *event) {
    // Write: [timestamp] [accessory_id] [buffer in hex]\n
    //        ^^^^^^^^^^^^^^^^^^^^^^^^^^
    //        This will be written first.

    // Summation of bytes:
    //  1 ~ 20 : int64_t timestamp (sc_tick is alias for int64_t).
    //  1 ~  1 : space separator
    //  1 ~  5 : uint16_t accessory_id
    // 3N ~ 3N : number of events * 3: space + 2 hex characters.
    //  1 ~  1 : '\n'
    //  1 ~  1 : NUL (not counted for data_len).
    // = bytes needed ranges from 5+3N to 28+3N. Allocate at least that many:
    size_t needed_event_size = 28 + event->size * 3;
    size_t needed_total_size = needed_event_size + hes->data_len;
    if (needed_total_size > hes->data_buffer_size) {
        hes->data = realloc(hes->data, needed_total_size);
        if (!hes->data) {
            LOG_OOM();
            return false;
        }
        hes->data_buffer_size = needed_total_size;
    }

    size_t data_len = hes->data_len;
    int start_size = snprintf(
            // Append after whatever that was written before:
            hes->data + data_len,
            // The previous realloc logic ensures that this is within bounds:
            needed_event_size,
            "%" PRItick " %" PRIu16, event->timestamp, event->accessory_id);
    assert(start_size > 0); // not -1 because our params are correct.
    assert((size_t)start_size < needed_event_size);
    data_len += start_size;
    for (unsigned i = 0; i < event->size; ++i) {
        snprintf(hes->data + data_len, 4, " %02x", event->buffer[i]);
        data_len += 3;
    }
    snprintf(hes->data + data_len, 2, "\n");
    ++data_len;

    hes->data_len = data_len;
    return true;
}

void
sc_hid_event_serializer_mark_as_read(struct sc_hid_event_serializer *hes) {
    hes->data_len = 0;
}
