/* [新增] 真正執行主迴圈，驗證 RPM/OLED 的呼叫順序，不依原始碼字串判斷。 */
#include <Arduino.h>
#include <cassert>
#include <vector>
#include <cstdio>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
static std::vector<char> calls;
uint64_t test_clock_us=100000U;
void (*test_interrupts[32])(void)={};
void brd_settings_begin() {}
void brd_io_begin() {}
void brd_battery_begin() {}
void brd_oled_begin() {}
void brd_battery_update() { calls.push_back('B'); }
bool brd_battery_measurement_allowed() { return true; }
bool brd_battery_restart_required() { return false; }
bool brd_bbp_prepare_restart() { return true; }
bool brd_bbp_begin() { calls.push_back('I'); return true; }
void brd_bbp_stop() { calls.push_back('X'); }
void brd_measurement_begin() { calls.push_back('A'); }
void brd_measurement_stop() { calls.push_back('D'); }
void brd_measurement_update() { calls.push_back('M'); }
bool brd_measurement_storage_allowed() { return true; }
bool brd_measurement_background_ready() { return true; }
void brd_oled_update() { calls.push_back('O'); }
bool brd_io_is_charging() { return false; }
bool brd_settings_bluetooth_allowed(bool) { return true; }
void brd_bbp_update(bool) { calls.push_back('T'); }
void brd_settings_ble_update() { calls.push_back('G'); }
void brd_settings_update() { calls.push_back('S'); }
bool brd_ota_is_busy() { return false; }
void brd_ota_update() { calls.push_back('U'); }
#include "../BRD_BPX.ino"
static size_t pos(char c) {
    for(size_t i=0;i<calls.size();i++) { if(calls[i]==c) { return i; } }
    return calls.size();
}
int main() {
    loop();
    assert(pos('M') < pos('B'));
    assert(pos('O') < pos('U') && pos('O') < pos('T') && pos('O') < pos('G') && pos('O') < pos('S'));
    for (char c : {'U','T','G','S'}) {
        size_t i=pos(c); assert(i+1<calls.size() && calls[i+1]=='A' && calls[i+2]=='M');
    }
    puts("PASS: measurement first, OLED before background, measurement serviced between background operations.");
}
