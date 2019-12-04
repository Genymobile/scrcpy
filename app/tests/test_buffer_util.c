#include <assert.h>

#include "util/buffer_util.h"

static void test_buffer_write16be(void) {
    uint16_t val = 0xABCD;
    uint8_t buf[2];

    buffer_write16be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
}

static void test_buffer_write32be(void) {
    uint32_t val = 0xABCD1234;
    uint8_t buf[4];

    buffer_write32be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
    assert(buf[2] == 0x12);
    assert(buf[3] == 0x34);
}

static void test_buffer_write64be(void) {
    uint64_t val = 0xABCD1234567890EF;
    uint8_t buf[8];

    buffer_write64be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
    assert(buf[2] == 0x12);
    assert(buf[3] == 0x34);
    assert(buf[4] == 0x56);
    assert(buf[5] == 0x78);
    assert(buf[6] == 0x90);
    assert(buf[7] == 0xEF);
}

static void test_buffer_read16be(void) {
    uint8_t buf[2] = {0xAB, 0xCD};

    uint16_t val = buffer_read16be(buf);

    assert(val == 0xABCD);
}

static void test_buffer_read32be(void) {
    uint8_t buf[4] = {0xAB, 0xCD, 0x12, 0x34};

    uint32_t val = buffer_read32be(buf);

    assert(val == 0xABCD1234);
}

static void test_buffer_read64be(void) {
    uint8_t buf[8] = {0xAB, 0xCD, 0x12, 0x34,
                      0x56, 0x78, 0x90, 0xEF};

    uint64_t val = buffer_read64be(buf);

    assert(val == 0xABCD1234567890EF);
}

int main(void) {
    test_buffer_write16be();
    test_buffer_write32be();
    test_buffer_write64be();
    test_buffer_read16be();
    test_buffer_read32be();
    test_buffer_read64be();
    return 0;
}
