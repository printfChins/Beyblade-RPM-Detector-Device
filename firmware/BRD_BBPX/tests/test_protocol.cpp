/* 檔案位置: BRD_BBP/tests/test_protocol.cpp
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
// [BRD_BBP 新增] 協定核心的獨立位元組 fixtures 與邊界測試。
#include "../brd_bbp_protocol.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static uint16_t le16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffffu;
}

static brd_bbp::Profile profile_with_raw(uint16_t raw)
{
    brd_bbp::Profile profile;
    profile.add_period_us(static_cast<uint32_t>(raw) * 8u);
    return profile;
}

static void test_header_fixture()
{
    brd_bbp::State state;
    brd_bbp::clear(state);
    state.total = 2;
    state.lifetime_max = 7309;
    const uint8_t uid[6] = {0x7b, 0x30, 0x03, 0x51, 0xc4, 0xda};
    const uint8_t expected[17] = {0xa0, 0x3a, 0, 0, 150, 0, 0, 0x8d, 0x1c,
                                  2, 0, 0x7b, 0x30, 0x03, 0x51, 0xc4, 0xda};
    uint8_t packet[17];
    brd_bbp::build_header(state, 0, 150, uid, packet);
    CHECK(memcmp(packet, expected, sizeof expected) == 0);
    brd_bbp::build_header(state, 0, 251, uid, packet);
    CHECK(packet[4] == 250);
}

static void test_profile_units_and_rounding()
{
    brd_bbp::Profile profile;
    profile.add_period_us(999);
    CHECK(profile.size() == 0);
    profile.add_period_us(6000);
    profile.add_period_us(6004);
    profile.add_period_us(6005);
    profile.add_period_us(1000000);
    profile.add_period_us(0xffffffffu);
    CHECK(profile.size() == 5);
    CHECK(profile.values()[0] == 750);
    CHECK(profile.values()[1] == 751);
    CHECK(profile.values()[2] == 751);
    CHECK(profile.values()[3] == 65535);
    CHECK(profile.values()[4] == 65535);
    CHECK(profile.peak_rpm() == 10000);
}

static void test_profile_first_32_frozen() {
    brd_bbp::Profile profile;
    for (uint16_t raw = 1000; raw < 1040; ++raw) {
        profile.add_period_us(static_cast<uint32_t>(raw) * 8U);
    }
    profile.add_period_us(4000);
    profile.add_period_us(3200);
    CHECK(profile.size() == 32);
    for (uint8_t i = 0; i < 32; ++i) {
        CHECK(profile.values()[i] == static_cast<uint16_t>(1000 + i));
    }
    CHECK(profile.peak_rpm() == 7500);
}

static void fill_state(brd_bbp::State &state, unsigned count)
{
    brd_bbp::clear(state);
    for (unsigned i = 1; i <= count; ++i) {
        brd_bbp::Profile profile = profile_with_raw(static_cast<uint16_t>(7500u - i));
        CHECK(brd_bbp::append(state, profile, profile.peak_rpm()));
    }
}

static void test_ring_boundaries_and_dump()
{
    const unsigned boundaries[] = {0, 1, 8, 48, 49, 50, 51, 105};
    for (size_t b = 0; b < sizeof boundaries / sizeof boundaries[0]; ++b) {
        brd_bbp::State state;
        fill_state(state, boundaries[b]);
        CHECK(state.total == boundaries[b]);
        CHECK(state.count == (boundaries[b] < 50 ? boundaries[b] : 50));
        CHECK(state.next == boundaries[b] % 50);
        uint8_t pages[12][17];
        brd_bbp::build_dump(state, pages);
        const unsigned kept = boundaries[b] < 50 ? boundaries[b] : 50;
        for (unsigned i = 0; i < kept; ++i) {
            const unsigned page = i / 8;
            const unsigned offset = 1 + (i % 8) * 2;
            const unsigned launch = boundaries[b] - kept + i + 1;
            CHECK(le16(&pages[page][offset]) == 7500000u / (7500u - launch));
        }
        CHECK(pages[6][5] == 0 && pages[6][6] == 0);
        CHECK(le16(&pages[6][7]) == state.lifetime_max);
        CHECK(le16(&pages[6][9]) == static_cast<uint16_t>(state.total));
        CHECK(pages[6][11] == state.count);
        unsigned sum = 0;
        for (unsigned page = 0; page < 7; ++page)
            for (unsigned byte = 1; byte < 17; ++byte)
                sum += pages[page][byte];
        CHECK(pages[7][16] == static_cast<uint8_t>(sum));
        if (boundaries[b] == 1)
            CHECK(pages[7][16] == 216); // 手算：B0 235 + B6 237。
    }
}

static void test_dump_curve_and_total_wrap()
{
    brd_bbp::State state;
    brd_bbp::clear(state);
    state.total = 65535;
    brd_bbp::Profile profile;
    for (uint16_t i = 0; i < 32; ++i)
        profile.add_period_us(static_cast<uint32_t>(1000 + i) * 8u);
    CHECK(brd_bbp::append(state, profile, profile.peak_rpm()));
    uint8_t pages[12][17];
    brd_bbp::build_dump(state, pages);
    CHECK(le16(&pages[6][9]) == 0);
    for (uint16_t i = 0; i < 32; ++i)
        CHECK(le16(&pages[8 + i / 8][1 + (i % 8) * 2]) == 1000 + i);
}

static void test_clear_and_empty_append()
{
    brd_bbp::State state;
    memset(&state, 0xa5, sizeof state);
    brd_bbp::clear(state);
    const uint8_t zero[sizeof state] = {};
    CHECK(memcmp(&state, zero, sizeof state) == 0);
    brd_bbp::Profile empty;
    CHECK(!brd_bbp::append(state, empty, 0));
    CHECK(memcmp(&state, zero, sizeof state) == 0);
}

static void test_storage_round_trip_and_crc()
{
    brd_bbp::State state;
    fill_state(state, 51);
    uint8_t storage[brd_bbp::STORAGE_SIZE];
    brd_bbp::encode_storage(state, storage);
    CHECK(memcmp(storage, "BBPS", 4) == 0 && storage[4] == 1);
    CHECK(crc32(storage, 177) == (static_cast<uint32_t>(storage[177]) |
          (static_cast<uint32_t>(storage[178]) << 8) |
          (static_cast<uint32_t>(storage[179]) << 16) |
          (static_cast<uint32_t>(storage[180]) << 24)));
    brd_bbp::State decoded;
    memset(&decoded, 0x5a, sizeof decoded);
    CHECK(brd_bbp::decode_storage(storage, sizeof storage, decoded));
    CHECK(memcmp(&state, &decoded, sizeof state) == 0);
}

static void rewrite_crc(uint8_t storage[brd_bbp::STORAGE_SIZE])
{
    const uint32_t crc = crc32(storage, 177);
    for (uint8_t i = 0; i < 4; ++i)
        storage[177 + i] = static_cast<uint8_t>(crc >> (8 * i));
}

static void test_storage_rejects_corruption_without_output_change()
{
    brd_bbp::State valid;
    fill_state(valid, 2);
    uint8_t base[brd_bbp::STORAGE_SIZE];
    brd_bbp::encode_storage(valid, base);
    for (unsigned kind = 0; kind < 10; ++kind) {
        uint8_t bad[brd_bbp::STORAGE_SIZE];
        memcpy(bad, base, sizeof bad);
        size_t length = sizeof bad;
        if (kind == 0) length--;
        if (kind == 1) bad[0] ^= 1;
        if (kind == 2) bad[4] = 2;
        if (kind == 3) bad[20] ^= 1;
        if (kind == 4) { bad[11] = 51; rewrite_crc(bad); }
        if (kind == 5) { bad[12] = 50; rewrite_crc(bad); }
        if (kind == 6) { bad[12] = 1; rewrite_crc(bad); }
        if (kind == 7) { bad[13] = 0x61; bad[14] = 0xea; rewrite_crc(bad); }
        if (kind == 8) { bad[9] = 0; bad[10] = 0; rewrite_crc(bad); }
        if (kind == 9) { bad[113] = 124; bad[114] = 0; rewrite_crc(bad); }
        brd_bbp::State output;
        memset(&output, 0xcc, sizeof output);
        brd_bbp::State before = output;
        CHECK(!brd_bbp::decode_storage(bad, length, output));
        CHECK(memcmp(&output, &before, sizeof output) == 0);
    }
}

static void test_storage_empty_and_curve_holes()
{
    brd_bbp::State empty;
    brd_bbp::clear(empty);
    uint8_t storage[brd_bbp::STORAGE_SIZE];
    brd_bbp::encode_storage(empty, storage);
    brd_bbp::State output;
    CHECK(brd_bbp::decode_storage(storage, sizeof storage, output));

    brd_bbp::State state;
    brd_bbp::clear(state);
    brd_bbp::Profile profile;
    profile.add_period_us(8000);
    profile.add_period_us(8008);
    CHECK(brd_bbp::append(state, profile, profile.peak_rpm()));
    brd_bbp::encode_storage(state, storage);
    storage[115] = 0; storage[116] = 0;
    storage[117] = 0xe8; storage[118] = 3;
    rewrite_crc(storage);
    CHECK(!brd_bbp::decode_storage(storage, sizeof storage, output));
}

int main()
{
    test_header_fixture();
    test_profile_units_and_rounding();
    test_profile_first_32_frozen();
    test_ring_boundaries_and_dump();
    test_dump_curve_and_total_wrap();
    test_clear_and_empty_append();
    test_storage_round_trip_and_crc();
    test_storage_rejects_corruption_without_output_change();
    test_storage_empty_and_curve_holes();
    if (failures != 0)
        fprintf(stderr, "%d test(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
