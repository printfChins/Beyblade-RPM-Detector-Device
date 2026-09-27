/* 檔案位置: BRD_BBPX/tests/test_measurement_auto.cpp
   [新增] AUTO 模式宿主驗證，不參與 Arduino 韌體編譯。 */
#define BRD_MEASUREMENT_MODE 1U

#include <cstdint>
#include <cstdio>
#include "host_stubs/soc/gpio_struct.h"
#include "../brd_bbp.h"

uint32_t host_micros = 0;
uint32_t host_millis = 0;
void (*host_interrupts[32])() = {};
host_gpio_t GPIO {};

#include "../brd_bbp_protocol.cpp"
#include "../brd_bbp_session.cpp"

static brd_bbp::Session host_session;
bool brd_bbp_begin() { return true; }
void brd_bbp_stop() {}
bool brd_bbp_prepare_restart() { return true; }
void brd_bbp_update(bool) {}
bool brd_bbp_is_connected() { return false; }
brd_bbp_diagnostics_t brd_bbp_get_diagnostics() { return {}; }
void brd_bbp_capture_reset() { host_session.reset_capture(); }
void brd_bbp_capture_period(uint32_t period_us, uint32_t event_us, bool record_profile) {
    host_session.period(period_us, event_us, record_profile);
}
void brd_bbp_capture_launch(uint32_t launch_us) { host_session.launched(launch_us); }
void brd_bbp_capture_finish() { host_session.finish(); }
void brd_bbp_capture_abort() { host_session.abort(); }

#include "../brd_measurement.cpp"

static int failures = 0;
#define CHECK(x) do { \
    if (!(x)) { \
        std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
        ++failures; \
    } \
} while (0)

static void at(uint32_t us) {
    host_micros = us;
    host_millis = us / 1000U;
    brd_measurement_update();
}

static void rpm(uint32_t us) {
    host_micros = us;
    host_interrupts[RPM_IR_GPIO]();
}

int main() {
    brd_measurement_begin();

    /* AUTO 不掛 LOAD 中斷，且 0 RPM 時 BLE loaded 為 false。 */
    CHECK(host_interrupts[LOAD_IR_GPIO] == nullptr);
    CHECK(host_interrupts[RPM_IR_GPIO] != nullptr);
    CHECK(!brd_measurement_get_display().loaded);
    CHECK(brd_measurement_storage_allowed());

    /* 1500 RPM < 2000；停止 250 ms 後視為有效歸零，不建立 Shot。 */
    rpm(1000U);
    at(1000U);
    rpm(41000U);
    at(41000U);
    CHECK(brd_measurement_get_display().value == 1500U);
    CHECK(brd_measurement_get_display().loaded);
    at(290999U);
    CHECK(brd_measurement_get_display().loaded);
    at(291000U);
    CHECK(brd_measurement_get_display().value == 0U);
    CHECK(!brd_measurement_get_display().loaded);
    CHECK(host_session.state().total == 0U);
    CHECK(brd_measurement_storage_allowed());

    /* 3000 RPM >= 2000；最後有效 RPM 後滿 1 秒才成立有效發射。 */
    rpm(300000U);
    at(300000U);
    rpm(320000U);
    at(320000U);
    rpm(340000U);
    at(340000U);
    CHECK(brd_measurement_get_display().value == 3000U);
    CHECK(brd_measurement_get_display().loaded);
    CHECK(host_session.state().total == 0U);

    at(1339999U);
    CHECK(brd_measurement_get_display().loaded);
    CHECK(host_session.state().total == 0U);

    at(1340000U);
    CHECK(!brd_measurement_get_display().loaded);
    CHECK(brd_measurement_get_display().show_max);
    CHECK(brd_measurement_get_display().hold_active);
    CHECK(brd_measurement_get_diagnostics().launch_events == 1U);

    host_session.update(1340000U, BBP_PUBLISH_MIN_DELAY_US);
    CHECK(host_session.state().total == 1U);
    CHECK(host_session.state().records[0] == 3000U);
    CHECK(host_session.state().curve[0] == 2500U);
    CHECK(host_session.state().curve[1] == 2500U);
    CHECK(host_session.state().curve[2] == 0U);

    /* HOLD 結束仍保留 MAX；下一次 RPM 真正開始才清除。 */
    at(3839999U);
    CHECK(brd_measurement_get_display().show_max);
    at(3840000U);
    CHECK(brd_measurement_get_display().show_max);
    CHECK(!brd_measurement_get_display().loaded);
    CHECK(brd_measurement_storage_allowed());

    rpm(3850000U);
    at(3850000U);
    CHECK(!brd_measurement_get_display().show_max);
    CHECK(!brd_measurement_get_display().loaded);
    rpm(3870000U);
    at(3870000U);
    CHECK(brd_measurement_get_display().value == 3000U);
    CHECK(brd_measurement_get_display().loaded);

    brd_measurement_stop();
    CHECK(host_interrupts[RPM_IR_GPIO] == nullptr);
    CHECK(host_interrupts[LOAD_IR_GPIO] == nullptr);
    return failures ? 1 : 0;
}
