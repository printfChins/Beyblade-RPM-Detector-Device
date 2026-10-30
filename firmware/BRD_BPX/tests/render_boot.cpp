/* [新增] 直接渲染真正 OLED framebuffer，供 128x32 排版視覺檢查。 */
#include <Arduino.h>
#include <assert.h>
#include "brd_settings.h"
#include "brd_config.h"
#include "brd_ota.h"
static brd_ota_display_t test_ota_display = {};
brd_ota_display_t brd_ota_get_display() { return test_ota_display; }
uint64_t test_clock_us=100000U;
void (*test_interrupts[32])(void)={};
bool brd_battery_is_low_locked() { return false; }
bool brd_battery_is_adc_fault() { return false; }
bool brd_battery_measurement_allowed() { return true; }
void brd_battery_update() {}
uint8_t brd_battery_get_percent() { return 80U; }
/* [新增] 真正 IR TEST OLED 模組所需的唯讀 stub。 */
uint16_t brd_battery_get_voltage_mv() { return 4021U; }
uint8_t brd_settings_ble_display_page() { return BRD_DISPLAY_PAGE_MAIN; }
bool brd_io_read_ir1() { return false; }
bool brd_io_read_ir2() { return false; }
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
    if(argc!=2 && argc!=3) { return 1; }
    brd_oled_begin();
    /* [V1.2 新增] 驗證扇形圓盤 0%、25%、50%、75%、100% 填充像素必須遞增。 */
    unsigned previous_pixels = 0U;
    for (unsigned percent = 0U; percent <= 100U; percent += 25U) {
        oled_clear_buffer();
        oled_draw_ota_sector(13U, 23U, 8U, static_cast<uint8_t>(percent));
        unsigned pixels = 0U;
        for (unsigned y = 15U; y < 32U; y++) {
            for (unsigned x = 5U; x <= 21U; x++) {
                pixels += ((g_oled_buffer[(y / 8U) * 128U + x] & (1U << (y % 8U))) != 0U) ? 1U : 0U;
            }
        }
        if (percent != 0U) { assert(pixels > previous_pixels); }
        previous_pixels = pixels;
        /* [V1.2 新增] 0% 必須為空心，100% 中心必須填滿。 */
        if (percent == 0U) { assert((g_oled_buffer[(23U / 8U) * 128U + 13U] & (1U << (23U % 8U))) == 0U); }
        if (percent == 100U) { assert((g_oled_buffer[(23U / 8U) * 128U + 13U] & (1U << (23U % 8U))) != 0U); }
    }

    if (argc == 3) {
        test_ota_display = {true, false, brd_ota::RECEIVING, 50U};
        /* [V1.2 新增] 模擬 OTA 各個進度，與單色 OLED framebuffer 對照。 */
        if (strncmp(argv[2], "ota", 3) == 0 && argv[2][3] != '\0') {
            unsigned v = 0U;
            for (const char *c = argv[2] + 3; *c >= '0' && *c <= '9'; c++) { v = v * 10U + static_cast<unsigned>(*c - '0'); }
            test_ota_display.percent = static_cast<uint8_t>(v > 100U ? 100U : v);
        }
        if (strcmp(argv[2], "verify") == 0) { test_ota_display.verifying = true; test_ota_display.percent = 99U; }
        if (strcmp(argv[2], "done") == 0) { test_ota_display.phase = brd_ota::COMPLETE; test_ota_display.percent = 100U; }
        /* [新增] 預覽 IR TEST 四種實心/空心組合，並檢查感測器位置。 */
        if (strncmp(argv[2], "ir", 2) == 0) {
            bool ir1 = strcmp(argv[2], "ir1") == 0 || strcmp(argv[2], "irboth") == 0;
            bool ir2 = strcmp(argv[2], "ir2") == 0 || strcmp(argv[2], "irboth") == 0;
            oled_begin_ir_test_frame(80U,4021U,false,ir1,ir2);
            /* [修改] 兩圖由 18px 間距改為 10px：各向中間移 4px。 */
            auto pixel=[](unsigned x,unsigned y) {
                return (g_oled_buffer[(y/8U)*128U+x] & (1U<<(y%8U)))!=0U;
            };
            /* [修改] 頂部左側維持 IR TEST；右側顯示 4.02V，不含 BAT。 */
            char voltage_text[12];
            oled_format_ir_voltage(4021U, voltage_text, sizeof(voltage_text));
            assert(strcmp(voltage_text, "4.02V") == 0);
            oled_format_ir_voltage(4124U, voltage_text, sizeof(voltage_text));
            assert(strcmp(voltage_text, "4.12V") == 0);
            oled_format_ir_voltage(4125U, voltage_text, sizeof(voltage_text));
            assert(strcmp(voltage_text, "4.13V") == 0);
            oled_format_ir_voltage(4199U, voltage_text, sizeof(voltage_text));
            assert(strcmp(voltage_text, "4.20V") == 0);
            /* [新增] 與單獨渲染的預期字型逐點比對右上區域。 */
            uint8_t actual_top[(OLED_WIDTH * OLED_HEIGHT) / 8U];
            memcpy(actual_top, g_oled_buffer, sizeof(actual_top));
            oled_clear_buffer();
            oled_draw_right_aligned_text(127U, 0U, "4.02V", 1U);
            for (unsigned y = 0U; y <= 6U; y++) {
                for (unsigned x = 48U; x < 128U; x++) {
                    bool expected = pixel(x, y);
                    bool actual = (actual_top[(y / 8U) * 128U + x] & (1U << (y % 8U))) != 0U;
                    assert(expected == actual);
                }
            }
            memcpy(g_oled_buffer, actual_top, sizeof(actual_top));
            /* [保留] 未觸發中心為空，觸發時中心填實。 */
            assert(pixel(50U,19U)==ir2);
            assert(pixel(76U,19U)==ir1);
            /* [修改] IR2 頂點：x 由 38..54 改成 42..58。 */
            assert(pixel(42U,19U));
            assert(pixel(58U,19U));
            assert(pixel(46U,12U));
            assert(pixel(54U,12U));
            assert(pixel(46U,26U));
            assert(pixel(54U,26U));
            /* [修改] IR1 六頂點：x 由 73..86 改成 69..82。 */
            assert(pixel(69U,12U));
            assert(pixel(69U,26U));
            assert(pixel(79U,30U));
            assert(pixel(82U,28U));
            assert(pixel(82U,10U));
            assert(pixel(79U,8U));
            /* [保留] 右圖「左短、右長」，不得增加左右鏡射。 */
            assert(!pixel(69U,10U));
            assert(pixel(82U,10U));
            assert(!pixel(69U,28U));
            assert(pixel(82U,28U));
            /* [保留] IR2 左右、上下對稱。 */
            for (unsigned y=12U; y<=26U; y++) {
                for (unsigned offset=0U; offset<=8U; offset++) {
                    assert(pixel(50U-offset,y)==pixel(50U+offset,y));
                }
            }
            for (unsigned x=42U; x<=58U; x++) {
                for (unsigned y=12U; y<=19U; y++) {
                    assert(pixel(x,y)==pixel(x,38U-y));
                }
            }
            /* [保留] IR1 僅上下鏡射。 */
            for (unsigned x=69U; x<=82U; x++) {
                for (unsigned y=8U; y<=19U; y++) {
                    assert(pixel(x,y)==pixel(x,38U-y));
                }
            }
            /* [新增] 嚴格確認兩圖外框間 10px 全空白。 */
            for (unsigned y=8U; y<=30U; y++) {
                for (unsigned x=59U; x<=68U; x++) {
                    assert(!pixel(x,y));
                }
            }
            /* [保留] 移除 IR1 / IR2 文字。 */
            assert(!pixel(50U,30U));
            assert(!pixel(76U,31U));
            FILE *file=fopen(argv[1],"wb");
            assert(file!=nullptr);
            fprintf(file,"P5\n128 32\n255\n");
            for(unsigned y=0;y<32;y++) {
                for(unsigned x=0;x<128;x++) {
                    uint8_t point=pixel(x,y) ? 255U : 0U;
                    fwrite(&point,1,1,file);
                }
            }
            fclose(file);
            puts("PASS: rotated DXF geometry, 10px gap and right-aligned 2dp voltage without BAT.");
            return 0;
        }
        for (unsigned i = 0; i < 80U; i++) { brd_oled_update(); }
        /*
            [新增] OTA 百分比 2 倍字型逐像素回歸測試。
            兩個小圈中央需保持黑色，四周封閉且存在獨立的斜線。
            以實際 OLED framebuffer 驗證，不只檢查字型常數。
        */
        if (strncmp(argv[2], "ota", 3) == 0 && argv[2][3] != '\0') {
            uint8_t percent = test_ota_display.percent;
            uint8_t digits = (percent == 100U) ? 3U : ((percent >= 10U) ? 2U : 1U);
            uint8_t characters = (uint8_t)(digits + 1U);
            uint8_t width = (uint8_t)(characters * 12U - 2U);
            uint8_t glyph_x = (uint8_t)((128U - width) / 2U + digits * 12U);
            auto pixel = [](unsigned x, unsigned y) {
                return (g_oled_buffer[(y / 8U) * 128U + x] & (1U << (y % 8U))) != 0U;
            };
            /* 上方空心圓圈及中心。 */
            assert(pixel(glyph_x + 1U, 16U));
            assert(pixel(glyph_x, 17U));
            assert(pixel(glyph_x + 3U, 18U));
            assert(!pixel(glyph_x + 1U, 17U));
            assert(!pixel(glyph_x + 2U, 18U));
            /* 下方空心圓圈及中心。 */
            assert(pixel(glyph_x + 7U, 26U));
            assert(pixel(glyph_x + 6U, 27U));
            assert(pixel(glyph_x + 9U, 28U));
            assert(!pixel(glyph_x + 7U, 27U));
            assert(!pixel(glyph_x + 8U, 28U));
            /* 中間斜線不能消失。 */
            assert(pixel(glyph_x + 6U, 22U));
            assert(pixel(glyph_x + 5U, 23U));
        }
        if (strcmp(argv[2], "done") == 0) {
            /* [保留] OTA DONE 的 N 位於 x=36，完成標籤不可顯示成空白。 */
            assert((g_oled_buffer[36] | g_oled_buffer[37] | g_oled_buffer[38] |
                    g_oled_buffer[39] | g_oled_buffer[40]) != 0U);
        }
    }
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
