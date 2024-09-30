#include "common.h"

#include <assert.h>

#include "util/binary.h"

static void test_write16be(void) {
    uint16_t val = 0xABCD;
    uint8_t buf[2];

    sc_write16be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
}

static void test_write32be(void) {
    uint32_t val = 0xABCD1234;
    uint8_t buf[4];

    sc_write32be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
    assert(buf[2] == 0x12);
    assert(buf[3] == 0x34);
}

static void test_write64be(void) {
    uint64_t val = 0xABCD1234567890EF;
    uint8_t buf[8];

    sc_write64be(buf, val);

    assert(buf[0] == 0xAB);
    assert(buf[1] == 0xCD);
    assert(buf[2] == 0x12);
    assert(buf[3] == 0x34);
    assert(buf[4] == 0x56);
    assert(buf[5] == 0x78);
    assert(buf[6] == 0x90);
    assert(buf[7] == 0xEF);
}

static void test_write16le(void) {
    uint16_t val = 0xABCD;
    uint8_t buf[2];

    sc_write16le(buf, val);

    assert(buf[0] == 0xCD);
    assert(buf[1] == 0xAB);
}

static void test_write32le(void) {
    uint32_t val = 0xABCD1234;
    uint8_t buf[4];

    sc_write32le(buf, val);

    assert(buf[0] == 0x34);
    assert(buf[1] == 0x12);
    assert(buf[2] == 0xCD);
    assert(buf[3] == 0xAB);
}

static void test_write64le(void) {
    uint64_t val = 0xABCD1234567890EF;
    uint8_t buf[8];

    sc_write64le(buf, val);

    assert(buf[0] == 0xEF);
    assert(buf[1] == 0x90);
    assert(buf[2] == 0x78);
    assert(buf[3] == 0x56);
    assert(buf[4] == 0x34);
    assert(buf[5] == 0x12);
    assert(buf[6] == 0xCD);
    assert(buf[7] == 0xAB);
}

static void test_read16be(void) {
    uint8_t buf[2] = {0xAB, 0xCD};

    uint16_t val = sc_read16be(buf);

    assert(val == 0xABCD);
}

static void test_read32be(void) {
    uint8_t buf[4] = {0xAB, 0xCD, 0x12, 0x34};

    uint32_t val = sc_read32be(buf);

    assert(val == 0xABCD1234);
}

static void test_read64be(void) {
    uint8_t buf[8] = {0xAB, 0xCD, 0x12, 0x34,
                      0x56, 0x78, 0x90, 0xEF};

    uint64_t val = sc_read64be(buf);

    assert(val == 0xABCD1234567890EF);
}

static void test_float_to_u16fp(void) {
    assert(sc_float_to_u16fp(0.0f) == 0);
    assert(sc_float_to_u16fp(0.03125f) == 0x800);
    assert(sc_float_to_u16fp(0.0625f) == 0x1000);
    assert(sc_float_to_u16fp(0.125f) == 0x2000);
    assert(sc_float_to_u16fp(0.25f) == 0x4000);
    assert(sc_float_to_u16fp(0.5f) == 0x8000);
    assert(sc_float_to_u16fp(0.75f) == 0xc000);
    assert(sc_float_to_u16fp(1.0f) == 0xffff);
}

static void test_float_to_i16fp(void) {
    assert(sc_float_to_i16fp(0.0f) == 0);
    assert(sc_float_to_i16fp(0.03125f) == 0x400);
    assert(sc_float_to_i16fp(0.0625f) == 0x800);
    assert(sc_float_to_i16fp(0.125f) == 0x1000);
    assert(sc_float_to_i16fp(0.25f) == 0x2000);
    assert(sc_float_to_i16fp(0.5f) == 0x4000);
    assert(sc_float_to_i16fp(0.75f) == 0x6000);
    assert(sc_float_to_i16fp(1.0f) == 0x7fff);

    assert(sc_float_to_i16fp(-0.03125f) == -0x400);
    assert(sc_float_to_i16fp(-0.0625f) == -0x800);
    assert(sc_float_to_i16fp(-0.125f) == -0x1000);
    assert(sc_float_to_i16fp(-0.25f) == -0x2000);
    assert(sc_float_to_i16fp(-0.5f) == -0x4000);
    assert(sc_float_to_i16fp(-0.75f) == -0x6000);
    assert(sc_float_to_i16fp(-1.0f) == -0x8000);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    test_write16be();
    test_write32be();
    test_write64be();
    test_read16be();
    test_read32be();
    test_read64be();

    test_write16le();
    test_write32le();
    test_write64le();

    test_float_to_u16fp();
    test_float_to_i16fp();
    return 0;
}
