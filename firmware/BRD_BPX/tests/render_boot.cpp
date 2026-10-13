/* [新增] 直接渲染真正 OLED framebuffer，供 128x32 排版視覺檢查。 */
#include <Arduino.h>
#include "brd_settings.h"
uint64_t test_clock_us=100000U;
void (*test_interrupts[32])(void)={};
bool brd_battery_is_low_locked() { return false; }
bool brd_battery_is_adc_fault() { return false; }
bool brd_battery_measurement_allowed() { return true; }
void brd_battery_update() {}
uint8_t brd_battery_get_percent() { return 80U; }
/* [新增] 配合 OLED 的 BLE 啟動狀態唯讀介面；開機預覽尚未啟動 BLE。 */
bool brd_bbp_is_started() { return false; }
bool brd_bbp_is_connected() { return false; }
bool brd_io_is_charging() { return false; }
const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values value={0U,1U,30U,2000U}; return value;
}
#include "../brd_oled.cpp"
brd_display_t brd_measurement_get_display() { return {}; }
void brd_measurement_max_frame_presented(uint32_t) {}
int main(int argc,char **argv) {
    if(argc!=2) { return 1; }
    brd_oled_begin();
    FILE *file=fopen(argv[1],"wb");
    if(file==nullptr) { return 1; }
    fprintf(file,"P5\n128 32\n255\n");
    for(unsigned y=0;y<32;y++) {
        for(unsigned x=0;x<128;x++) {
            uint8_t pixel=(g_oled_buffer[(y/8U)*128U+x] & (1U<<(y%8U))) ? 255U : 0U;
            fwrite(&pixel,1,1,file);
        }
    }
    fclose(file);
    puts("PASS: actual OLED boot framebuffer rendered.");
}
