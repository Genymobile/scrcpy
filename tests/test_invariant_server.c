#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Simulate the device_name field size as used in scrcpy */
#define DEVICE_NAME_FIELD_LENGTH 64

/* Simulated server_info structure */
struct server_info {
    char device_name[DEVICE_NAME_FIELD_LENGTH];
};

/*
 * Safe version of the copy operation that enforces null-termination.
 * This is what the fix should look like.
 */
static void safe_copy_device_name(struct server_info *info, const char *buf, size_t buf_len) {
    size_t copy_len = buf_len < sizeof(info->device_name) ? buf_len : sizeof(info->device_name);
    memcpy(info->device_name, buf, copy_len);
    /* INVARIANT: device_name must always be null-terminated */
    info->device_name[sizeof(info->device_name) - 1] = '\0';
}

/*
 * Validate that a device_name buffer is properly null-terminated
 * within its bounds.
 */
static int is_null_terminated_within_bounds(const char *buf, size_t buf_size) {
    for (size_t i = 0; i < buf_size; i++) {
        if (buf[i] == '\0') {
            return 1;
        }
    }
    return 0;
}

/*
 * Validate that strlen on the device_name won't exceed the buffer boundary.
 */
static int string_length_within_bounds(const char *buf, size_t buf_size) {
    if (!is_null_terminated_within_bounds(buf, buf_size)) {
        return 0;
    }
    return strnlen(buf, buf_size) < buf_size;
}

START_TEST(test_device_name_null_termination_invariant)
{
    /* Invariant: device_name must always be null-terminated within its buffer
     * regardless of what data is received over the network socket */

    /* Adversarial payloads: buffers without null terminators, overlong strings,
     * binary data, etc. */
    static const unsigned char payload_no_null[DEVICE_NAME_FIELD_LENGTH] = {
        'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A'
    };

    static const unsigned char payload_all_ff[DEVICE_NAME_FIELD_LENGTH] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    static const unsigned char payload_binary_mixed[DEVICE_NAME_FIELD_LENGTH] = {
        0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,
        0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,
        0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,
        0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF,0x41,0x00,0x41,0xFF
    };

    static const unsigned char payload_format_string[DEVICE_NAME_FIELD_LENGTH] = {
        '%','s','%','s','%','s','%','s','%','s','%','s','%','s','%','s',
        '%','n','%','n','%','n','%','n','%','n','%','n','%','n','%','n',
        '%','x','%','x','%','x','%','x','%','x','%','x','%','x','%','x',
        '%','p','%','p','%','p','%','p','%','p','%','p','%','p','%','p'
    };

    static const unsigned char payload_null_at_end[DEVICE_NAME_FIELD_LENGTH] = {
        'E','V','I','L','_','D','E','V','I','C','E','_','N','A','M','E',
        'E','V','I','L','_','D','E','V','I','C','E','_','N','A','M','E',
        'E','V','I','L','_','D','E','V','I','C','E','_','N','A','M','E',
        'E','V','I','L','_','D','E','V','I','C','E','_','N','A','M','\0'
    };

    static const unsigned char payload_overflow_attempt[DEVICE_NAME_FIELD_LENGTH + 32];  /* zero-initialized */

    static const unsigned char payload_sql_injection[DEVICE_NAME_FIELD_LENGTH] = {
        '\'',';','D','R','O','P',' ','T','A','B','L','E',' ','u','s','e',
        'r','s',';','-','-','\'',';','D','R','O','P',' ','T','A','B','L',
        'E',' ','u','s','e','r','s',';','-','-','\'',';','D','R','O','P',
        ' ','T','A','B','L','E',' ','u','s','e','r','s',';','-','-','!'
    };

    static const unsigned char payload_path_traversal[DEVICE_NAME_FIELD_LENGTH] = {
        '.','.','/','.','.','/','.','.','/','.','.','/','.','.','/','e',
        't','c','/','p','a','s','s','w','d','\x00','.','.','/','.','.',
        '/','.','.','/','.','.','/','.','.','/','.','.','/','.','.','/',
        '.','.','/','.','.','/','.','.','/','.','.','/','e','t','c','/'
    };

    static const unsigned char payload_unicode_overlong[DEVICE_NAME_FIELD_LENGTH] = {
        0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,0xC0,0xAF,
        0xE0,0x80,0xAF,0xE0,0x80,0xAF,0xE0,0x80,0xAF,0xE0,0x80,0xAF,0xE0,0x80,0xAF,0xE0,
        0xF0,0x80,0x80,0xAF,0xF0,0x80,0x80,0xAF,0xF0,0x80,0x80,0xAF,0xF0,0x80,0x80,0xAF,
        0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF,0xFE,0xFF
    };

    const struct {
        const unsigned char *data;
        size_t len;
        const char *description;
    } payloads[] = {
        { payload_no_null,        DEVICE_NAME_FIELD_LENGTH,      "no null terminator" },
        { payload_all_ff,         DEVICE_NAME_FIELD_LENGTH,      "all 0xFF bytes" },
        { payload_binary_mixed,   DEVICE_NAME_FIELD_LENGTH,      "binary mixed data" },
        { payload_format_string,  DEVICE_NAME_FIELD_LENGTH,      "format string attack" },
        { payload_null_at_end,    DEVICE_NAME_FIELD_LENGTH,      "null only at last byte" },
        { payload_overflow_attempt, DEVICE_NAME_FIELD_LENGTH,    "overflow attempt" },
        { payload_sql_injection,  DEVICE_NAME_FIELD_LENGTH,      "SQL injection payload" },
        { payload_path_traversal, DEVICE_NAME_FIELD_LENGTH,      "path traversal payload" },
        { payload_unicode_overlong, DEVICE_NAME_FIELD_LENGTH,    "unicode overlong encoding" },
    };

    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        struct server_info info;
        memset(&info, 0xAA, sizeof(info)); /* poison memory to detect uninitialized reads */

        /* Apply the safe copy operation */
        safe_copy_device_name(&info, (const char *)payloads[i].data, payloads[i].len);

        /* INVARIANT 1: device_name must be null-terminated within its buffer */
        ck_assert_msg(
            is_null_terminated_within_bounds(info.device_name, sizeof(info.device_name)),
            "INVARIANT VIOLATED: device_name not null-terminated for payload: %s",
            payloads[i].description
        );

        /* INVARIANT 2: strlen must not exceed buffer boundary */
        ck_assert_msg(
            string_length_within_bounds(info.device_name, sizeof(info.device_name)),
            "INVARIANT VIOLATED: string length exceeds buffer for payload: %s",
            payloads[i].description
        );

        /* INVARIANT 3: strnlen result must be strictly less than buffer size */
        size_t safe_len = strnlen(info.device_name, sizeof(info.device_name));
        ck_assert_msg(
            safe_len < sizeof(info.device_name),
            "INVARIANT VIOLATED: strnlen(%zu) >= buffer size(%zu) for payload: %s",
            safe_len, sizeof(info.device_name), payloads[i].description
        );

        /* INVARIANT 4: The last byte of device_name must always be '\0'
         * as a hard safety guarantee */
        ck_assert_msg(
            info.device_name[sizeof(info.device_name) - 1] == '\0',
            "INVARIANT VIOLATED: last byte of device_name is not null for payload: %s",
            payloads[i].description
        );

        /* INVARIANT 5: No out-of-bounds read should occur when treating as C string.
         * Verify that memchr finds a null within bounds (equivalent to safe string use). */
        ck_assert_msg(
            memchr(info.device_name, '\0', sizeof(info.device_name)) != NULL,
            "INVARIANT VIOLATED: no null byte found within device_name bounds for payload: %s",
            payloads[i].description
        );
    }
}
END_TEST

START_TEST(test_device_name_boundary_lengths)
{
    /* Invariant: device_name remains null-terminated for all boundary-length inputs */

    struct server_info info;

    /* Test with empty input */
    memset(&info, 0xBB, sizeof(info));
    safe_copy_device_name(&info, "", 0);
    ck_assert_msg(
        info.device_name[sizeof(info.device_name) - 1] == '\0',
        "INVARIANT VIOLATED: last byte not null for empty input"
    );

    /* Test with exactly DEVICE_NAME_FIELD_LENGTH - 1 bytes (no null) */
    char exact_minus_one[DEVICE_NAME_FIELD_LENGTH - 1];
    memset(exact_minus_one, 'X', sizeof(exact_minus_one));
    memset(&info, 0xBB, sizeof(info));
    safe_copy_device_name(&info, exact_minus_one, sizeof(exact_minus_one));
    ck_assert_msg(
        is_null_terminated_within_bounds(info.device_name, sizeof(info.device_name)),
        "INVARIANT VIOLATED: not null-terminated for FIELD_LENGTH-1 input"
    );

    /* Test with exactly DEVICE_NAME_FIELD_LENGTH bytes (no null) */
    char exact_full[DEVICE_NAME_FIELD_LENGTH];
    memset(exact_full, 'Y', sizeof(exact_full));
    memset(&info, 0xBB, sizeof(info));
    safe_copy_device_name(&info, exact_full, sizeof(exact_full));
    ck_assert_msg(
        info.device_name[sizeof(info.device_name) - 1] == '\0',
        "INVARIANT VIOLATED: last byte not null for exact FIELD_LENGTH input"
    );
    ck_assert_msg(
        is_null_terminated_within_bounds(info.device_name, sizeof(info.device_name)),
        "INVARIANT VIOLATED: not null-terminated for exact FIELD_LENGTH input"
    );

    /* Test with oversized input (simulating attacker sending more data) */
    char oversized[DEVICE_NAME_FIELD_LENGTH * 2];
    memset(oversized, 'Z', sizeof(oversized));
    memset(&info, 0xBB, sizeof(info));
    safe_copy_device_name(&info, oversized, sizeof(oversized));
    ck_assert_msg(
        info.device_name[sizeof(info.device_name) - 1] == '\0',
        "INVARIANT VIOLATED: last byte not null for oversized input"
    );
    ck_assert_msg(
        is_null_terminated_within_bounds(info.device_name, sizeof(info.device_name)),
        "INVARIANT VIOLATED: not null-terminated for oversized input"
    );
}
END_TEST

START_TEST(test_device_name_no_buffer_overread)
{
    /* Invariant: using device_name as a C string must not read beyond buffer boundary */

    struct server_info info;

    /* Create a buffer with no null terminator and poison surrounding memory */
    unsigned char test_region[sizeof(struct server_info) + 128];
    memset(test_region, 0xCC, sizeof(test_region)); /* poison */

    struct server_info *test_