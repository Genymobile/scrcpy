#include "common.h"

#include <assert.h>

#include "usb/hid_event_serializer.h"

static void test_hid_event_serializer(void) {
    struct sc_hid_event hid_event;
    uint16_t accessory_id = 1337;
    unsigned char *buffer = malloc(5);
    buffer[0] = '\xDE';
    buffer[1] = '\xEA';
    buffer[2] = '\xBE';
    buffer[3] = '\xEF';
    buffer[4] = '\x00';
    uint16_t buffer_size = 5;
    sc_hid_event_init(&hid_event, accessory_id, buffer, buffer_size);
    assert(hid_event.timestamp == 0);

    struct sc_hid_event_serializer hes;
    sc_hid_event_serializer_init(&hes);

    assert(hes.data_len == 0);

    sc_hid_event_serializer_update(&hes, &hid_event);
    assert(strlen("0 1337 de ea be ef 00\n") == hes.data_len);
    assert(hes.data_len < hes.data_buffer_size); // Need room for NUL.
    assert(!strncmp("0 1337 de ea be ef 00\n", hes.data, hes.data_len + 1));

    hid_event.timestamp = 9001;
    sc_hid_event_serializer_update(&hes, &hid_event);
    assert(!strncmp("0 1337 de ea be ef 00\n9001 1337 de ea be ef 00\n",
                hes.data, hes.data_len + 1));

    sc_hid_event_serializer_mark_as_read(&hes);
    assert(hes.data_len == 0);

    sc_hid_event_serializer_update(&hes, &hid_event);
    assert(!strncmp("9001 1337 de ea be ef 00\n", hes.data, hes.data_len + 1));

    sc_hid_event_serializer_destroy(&hes);
    free(buffer);
}

static void test_hid_event_serializer_only_init_and_destroy(void) {
    struct sc_hid_event_serializer hes;
    sc_hid_event_serializer_init(&hes);
    sc_hid_event_serializer_destroy(&hes);
}

static void test_hid_event_serializer_minimum_length(void) {
    struct sc_hid_event hid_event;
    sc_hid_event_init(&hid_event, 2, calloc(1, 1), 1);
    assert(hid_event.timestamp == 0);

    struct sc_hid_event_serializer hes;
    sc_hid_event_serializer_init(&hes);
    sc_hid_event_serializer_update(&hes, &hid_event);

    assert(hes.data_len < hes.data_buffer_size); // Need room for NUL.
    assert(strlen("0 2 00\n") == hes.data_len);
    assert(!strncmp("0 2 00\n", hes.data, hes.data_len + 1));

    sc_hid_event_serializer_destroy(&hes);
    free(hid_event.buffer);
}

static void test_hid_event_serializer_maximum_length(void) {
    struct sc_hid_event hid_event;
    sc_hid_event_init(&hid_event, 65535, calloc(1, 1), 1);
    // As the type is a signed 64-bit integer, the largest length is its lowest
    // value. It is unlikely for such a timestamp to be seen in practice.
    hid_event.timestamp = -9223372036854775807; // = -(2^63-1)

    struct sc_hid_event_serializer hes;
    sc_hid_event_serializer_init(&hes);
    sc_hid_event_serializer_update(&hes, &hid_event);

    // Now perform an exact check instead of a "data_len < buffer_size". The
    // minimum buffer size is carefully chosen to fit the longest values.
    assert(hes.data_len + 1 == hes.data_buffer_size);

    assert(hes.data_len < hes.data_buffer_size); // Need room for NUL.
    assert(strlen("-9223372036854775807 65535 00\n") == hes.data_len);
    assert(!strncmp("-9223372036854775807 65535 00\n", hes.data,
                hes.data_len + 1));

    sc_hid_event_serializer_destroy(&hes);
    free(hid_event.buffer);
}

static void test_hid_event_parser(void) {
    struct sc_hid_event_parser hep;
    sc_hid_event_parser_init(&hep, "source_name");

    const char input_str[] = "1 1023 f0 0d\n";
    int input_len = strlen(input_str);
    // Note: allocate just enough to hold input_str, without trailing NUL byte,
    // to show that the NUL byte is not required (even though C strings will
    // always end with a NUL).
    char *data = malloc(input_len);
    memcpy(data, input_str, input_len);
    bool ok = sc_hid_event_parser_append_data(&hep, data, input_len);
    assert(ok);
    free(data); // Free immediately, further operations should not trigger UAF.

    sc_hid_event_parser_parse_all_data(&hep);
    assert(sc_hid_event_parser_has_next(&hep));
    struct sc_hid_event *parsed_event = sc_hid_event_parser_get_next(&hep);
    assert(parsed_event);
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    assert(parsed_event->timestamp == 1);
    assert(parsed_event->accessory_id == 1023);
    assert(parsed_event->size == 2);
    assert(!strncmp((const char*)parsed_event->buffer, "\xf0\x0d", 2));

    // Clean up.
    sc_hid_event_destroy(parsed_event);
    free(parsed_event);

    // Another one, partial.
    ok = sc_hid_event_parser_append_data(&hep, "7", 1);
    assert(ok);
    sc_hid_event_parser_parse_all_data(&hep);
    assert(!sc_hid_event_parser_has_next(&hep));
    parsed_event = sc_hid_event_parser_get_next(&hep);
    assert(!parsed_event); // Incomplete.
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(sc_hid_event_parser_has_unparsed_data(&hep));

    // Append part of the original line (minus '\n')
    ok = sc_hid_event_parser_append_data(&hep, input_str, input_len - 1);
    assert(ok);
    // Append the new ending.
    ok = sc_hid_event_parser_append_data(&hep, " DE ED\n", strlen(" DE ED\n"));
    assert(ok);

    // The event is not seen until sc_hid_event_parser_parse_all_data() is run:
    parsed_event = sc_hid_event_parser_get_next(&hep);
    assert(!parsed_event);

    sc_hid_event_parser_parse_all_data(&hep);
    parsed_event = sc_hid_event_parser_get_next(&hep);
    assert(parsed_event);
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    assert(parsed_event->timestamp == 71);
    assert(parsed_event->accessory_id == 1023);
    assert(parsed_event->size == 4);
    assert(!strncmp((const char*)parsed_event->buffer, "\xf0\x0d\xde\xed", 4));

    sc_hid_event_parser_destroy(&hep);

    // Clean up once more, now after hep has been destroyed to prove that the
    // parsed_event outlives the parser.
    sc_hid_event_destroy(parsed_event);
    free(parsed_event);
}

static void test_hid_event_parser_only_init_and_destroy(void) {
    struct sc_hid_event_parser hep;
    sc_hid_event_parser_init(&hep, "source_name");
    sc_hid_event_parser_destroy(&hep);
}

static void test_hid_event_parser_reject_null_in_input(void) {
    struct sc_hid_event_parser hep;
    sc_hid_event_parser_init(&hep, "invalid_embedded_nulls");

    const size_t input_len = 14;
    char input_with_nul[15] = {0};
    memcpy(input_with_nul + 1, "1 1023 f0 0d\n", 14);

    // Note: data is not empty but data_len is 0, so the \x00 should be ignored.
    bool ok = sc_hid_event_parser_append_data(&hep, input_with_nul, 0);
    assert(ok); // No data to append, all right!
    sc_hid_event_parser_parse_all_data(&hep);
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, input_with_nul, input_len);
    assert(!ok); // Invalid due to null.
    // The error is not propagated until sc_hid_event_parser_parse_all_data():
    assert(!sc_hid_event_parser_has_error(&hep));
    // The "has unparsed data" status is immediately updated.
    assert(sc_hid_event_parser_has_unparsed_data(&hep));

    sc_hid_event_parser_parse_all_data(&hep);

    // Confirm that once an error is reached, that parsing fails too.
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(sc_hid_event_parser_has_error(&hep));
    assert(sc_hid_event_parser_has_unparsed_data(&hep));

    sc_hid_event_parser_destroy(&hep);
}

static void test_hid_event_parser_invalid_data(void) {
    struct sc_hid_event_parser hep;
    sc_hid_event_parser_init(&hep, "invalid_source_data");

    bool ok = sc_hid_event_parser_append_data(&hep, "", 0);
    assert(ok); // No data to append, all right!
    sc_hid_event_parser_parse_all_data(&hep);
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));
    
    ok = sc_hid_event_parser_append_data(&hep, "", 0);
    assert(ok); // No data to append, still all right!
    sc_hid_event_parser_parse_all_data(&hep);
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, "Clearly bogus\n", 14);
    assert(ok); // Garbage accepted - append does not validate.
    sc_hid_event_parser_parse_all_data(&hep);
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(sc_hid_event_parser_has_error(&hep));
    assert(sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, "", 0);
    assert(!ok); // No more data accepted when garbage is encountered.

    sc_hid_event_parser_destroy(&hep);
}

static void test_hid_event_parser_skips_comments_and_lines(void) {
    struct sc_hid_event_parser hep;
    sc_hid_event_parser_init(&hep, "source_name");

    bool ok = sc_hid_event_parser_append_data(&hep, "\n\n\n\n\n", 5);
    assert(ok);
    assert(hep.line == 1); // Not parsed yet.
    sc_hid_event_parser_parse_all_data(&hep);
    assert(hep.line == 6); // Skipped all blank lines.
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, "#", 1);
    assert(ok);
    sc_hid_event_parser_parse_all_data(&hep);
    assert(hep.line == 6); // Line not changed.
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, "xxx\n", 4);
    assert(ok);
    assert(hep.line == 6); // Not parsed yet.
    sc_hid_event_parser_parse_all_data(&hep);
    assert(hep.line == 7); // Line consumed & ignored.
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    ok = sc_hid_event_parser_append_data(&hep, "#\n", 2);
    assert(ok);
    sc_hid_event_parser_parse_all_data(&hep);
    assert(hep.line == 8); // # immediately followed by \n is also ignored.
    assert(!sc_hid_event_parser_get_next(&hep));
    assert(!sc_hid_event_parser_has_error(&hep));
    assert(!sc_hid_event_parser_has_unparsed_data(&hep));

    sc_hid_event_parser_destroy(&hep);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_hid_event_serializer();
    test_hid_event_serializer_only_init_and_destroy();
    test_hid_event_serializer_minimum_length();
    test_hid_event_serializer_maximum_length();

    test_hid_event_parser();
    test_hid_event_parser_only_init_and_destroy();
    test_hid_event_parser_reject_null_in_input();
    test_hid_event_parser_invalid_data();
    test_hid_event_parser_skips_comments_and_lines();

    return 0;
}
