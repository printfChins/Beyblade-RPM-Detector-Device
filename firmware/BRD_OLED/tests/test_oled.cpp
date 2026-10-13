/* [新增] 驗證實際 OLED 畫面只顯示量測、電池與充電資訊。 */
#include <Arduino.h>
#include <cstdio>
#include <cassert>
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
static bool test_charging = false;
bool brd_battery_is_low_locked() { return false; }
bool brd_battery_is_adc_fault() { return false; }
uint8_t brd_battery_get_percent() { return 80U; }
bool brd_io_is_charging() { return test_charging; }
#include "../brd_oled.cpp"
brd_display_t brd_measurement_get_display() { return {}; }
void brd_measurement_max_frame_presented(uint32_t) {}
static bool pixel_on(unsigned x, unsigned y) {
    return (g_oled_buffer[(y / 8U) * OLED_WIDTH + x] & (1U << (y % 8U))) != 0U;
}
static bool check_measurement_frame(bool charging, bool show_max) {
    test_charging = charging;
    brd_display_t display = {true,show_max,30000U,1U,show_max};
    oled_begin_frame(display,80U,charging);
    for (unsigned y = 16U; y < OLED_HEIGHT; y++) {
        for (unsigned x = 111U; x < OLED_WIDTH; x++) {
            if (pixel_on(x,y)) {
                fprintf(stderr,"FAIL: unexpected indicator at (%u,%u), charging=%u\n",x,y,charging);
                return false;
            }
        }
    }
    assert(pixel_on(109U,0U));
    return true;
}
int main() {
    if (!check_measurement_frame(false,false) || !check_measurement_frame(true,true)) {
        return 1;
    }
    puts("PASS: actual RPM/MAX framebuffer has no wireless indicators; battery remains.");
}
