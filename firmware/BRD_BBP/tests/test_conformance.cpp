/* 檔案位置: BRD_BBP/tests/test_conformance.cpp
   [R2 修改] 依最新需求驗證裝載後記錄、前 32 點及獨立 History。 */
#include "../brd_bbp_session.h"
#include <cstdio>
#include <cstring>

static int failures;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void launch(brd_bbp::Session &session, uint32_t timestamp) {
    session.launched(timestamp);
}

static void test_first_32_survive_late_peak() {
    brd_bbp::Profile profile;
    for (uint32_t i = 0; i < 40; ++i) {
        profile.add_period_us((1000U + i) * 8U);
    }
    profile.add_period_us(4000);
    CHECK(profile.size() == 32);
    for (uint8_t i = 0; i < 32; ++i) {
        CHECK(profile.values()[i] == 1000U + i);
    }
    CHECK(profile.peak_rpm() == 7500);
}

static void test_loaded_profile_survives_launch() {
    brd_bbp::Session session;
    session.period(4000, 10000); // 裝載後首筆有效值，即使在發射前也要保存。
    launch(session, 12000);
    session.period(8000, 18000); // 整圈跨越發射點仍保留，Launch 不重置。
    session.period(12000, 30000);
    session.period(16000, 46000);
    session.finish();
    session.update(610000, 600000);
    CHECK(session.state().total == 1);
    CHECK(session.state().records[0] == 15000);
    CHECK(session.state().curve[0] == 500);
    CHECK(session.state().curve[1] == 1000);
    CHECK(session.state().curve[2] == 1500);
    CHECK(session.state().curve[3] == 2000);
    CHECK(session.state().curve[4] == 0);
}

static void test_no_post_launch_pulses_keeps_loaded_profile() {
    brd_bbp::Session session;
    session.period(6000, 10000);
    launch(session, 12000);
    session.finish();
    session.update(610000, 600000);
    CHECK(session.state().total == 1 && session.state().records[0] == 10000);
    CHECK(session.state().curve[0] == 750);
    for (uint8_t i = 1; i < 32; ++i) {
        CHECK(session.state().curve[i] == 0);
    }
}

static void test_late_peak_updates_only_history() {
    brd_bbp::Session session;
    uint32_t event = 100000;
    launch(session, event);
    for (unsigned i = 0; i < 32; ++i) {
        event += 8000;
        session.period(8000, event);
    }
    event += 4000;
    session.period(4000, event);
    session.finish();
    session.update(708000, 600000);
    CHECK(session.state().records[0] == 15000);
    for (uint8_t i = 0; i < 32; ++i) {
        CHECK(session.state().curve[i] == 1000);
    }
}

static void test_storage_independent_and_empty_curve() {
    brd_bbp::State state, restored;
    brd_bbp::clear(state);
    state.total = state.count = state.next = 1;
    state.records[0] = state.lifetime_max = 15000;
    state.curve[0] = 1500;
    uint8_t blob[brd_bbp::STORAGE_SIZE];
    brd_bbp::encode_storage(state, blob);
    CHECK(brd_bbp::decode_storage(blob, sizeof blob, restored));
    state.curve[0] = 0;
    brd_bbp::encode_storage(state, blob);
    CHECK(brd_bbp::decode_storage(blob, sizeof blob, restored));
    CHECK(std::memcmp(&state, &restored, sizeof state) == 0);
}

static void test_capture_survives_launch_and_micros_wrap() {
    brd_bbp::Session session;
    session.period(4000, 0xfffff000U);
    launch(session, 0xffffff00U);
    session.period(8192, 0x00001000U); // 跨越發射點的週期仍記錄。
    session.period(8192, 0x00003000U);
    session.finish();
    session.update(0x000a0000U, 600000);
    CHECK(session.state().total == 1);
    CHECK(session.state().records[0] == 15000);
    CHECK(session.state().curve[0] == 500 && session.state().curve[1] == 1024);
    CHECK(session.state().curve[2] == 1024 && session.state().curve[3] == 0);
}

int main() {
    test_first_32_survive_late_peak();
    test_loaded_profile_survives_launch();
    test_no_post_launch_pulses_keeps_loaded_profile();
    test_late_peak_updates_only_history();
    test_storage_independent_and_empty_curve();
    test_capture_survives_launch_and_micros_wrap();
    if (failures != 0) {
        std::fprintf(stderr, "%d conformance checks failed\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
