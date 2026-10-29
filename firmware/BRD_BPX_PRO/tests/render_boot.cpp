/* [PRO 新增] 以真正 ST7735S RGB565 framebuffer 產生 160x80 主機預覽，驗證版面及 OTA。 */
#include <Arduino.h>
#include <assert.h>
#include <cstdio>
#include <cstring>
#include "brd_settings.h"
#include "brd_config.h"
#include "brd_ota.h"
#include "brd_measurement.h"
static brd_ota_display_t test_ota_display={};
static uint8_t test_page = BRD_DISPLAY_PAGE_MAIN;
static bool test_ir1=false,test_ir2=false;
static bool test_low=false,test_adc_fault=false;
static bool test_button=false;
static bool test_charging=false;
static bool test_ble_connected=true;
uint64_t test_clock_us=100000U;
void (*test_interrupts[32])(void)={};
brd_ota_display_t brd_ota_get_display() { return test_ota_display; }
bool brd_battery_is_low_locked() {return test_low;}
bool brd_battery_is_adc_fault() {return test_adc_fault;}
uint8_t brd_battery_get_percent() {return 80U;}
uint16_t brd_battery_get_voltage_mv() {return 4125U;}
uint8_t brd_settings_ble_display_page() {return test_page;}
bool brd_io_read_ir1() {return test_ir1;}
bool brd_io_read_ir2() {return test_ir2;}
void brd_load_adc_update() {}
bool brd_load_adc_valid() {return true;}
uint16_t brd_load_adc_raw() {return test_ir2? 3200U : 890U;}
void brd_rpm_adc_begin() {}
void brd_rpm_adc_update() {}
bool brd_rpm_adc_valid() {return true;}
uint16_t brd_rpm_adc_raw() {return test_ir1? 3500U : 800U;}
bool brd_bbp_is_started() {return true;}
bool brd_bbp_is_connected() {return test_ble_connected;}
bool brd_io_is_charging() {return test_charging;}
bool brd_io_button_pressed() {return test_button;}
const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values v={0U,0U,30U,2000U}; return v;
}
static brd_display_t current_display={true,false,30000U,5U,false};
brd_display_t brd_measurement_get_display() {return current_display;}
void brd_measurement_max_frame_presented(uint32_t) {}
/* [修改] 實際 renderer 現在分多個模組，透過公開 UI Screen API 驗證。 */
#include "brd_tft.h"
#include "brd_ui_screen.h"
#include "brd_ui_core.h"
#include "brd_ui_curve.h"
static bool set_ota_mode(const char *name) {
    if(strncmp(name,"ota",3)==0) {
        test_ota_display={true,false,brd_ota::RECEIVING,50U};
        if(name[3]!='\0') {
            unsigned v=0;
            for(const char *s=name+3; *s>='0'&&*s<='9'; s++) {v=v*10U+static_cast<unsigned>(*s-'0');}
            test_ota_display.percent=static_cast<uint8_t>(v>100U?100U:v);
        }
        return true;
    }
    if(strcmp(name,"verify")==0) {test_ota_display={true,true,brd_ota::RECEIVING,99U};return true;}
    if(strcmp(name,"done")==0) {test_ota_display={true,false,brd_ota::COMPLETE,100U};return true;}
    return false;
}
int main(int argc,char **argv) {
    if(argc!=2 && argc!=3) {return 1;}
    brd_tft_begin();
    /* [修改] GFX 直接繪圖；正式韌體無自製 framebuffer、flush 或手工字庫。 */
    assert(brd_ui::ready());
    assert(brd_ui::lcd().init_count == 2U);
    assert(brd_ui::lcd().full_clear_count >= 1U);
    /* [修改] 各頁預覽的像素皆取自 UI Core 的官方 GFX 測試面板。 */
    if(argc==3) {
        if(strcmp(argv[2],"low")==0) {test_low=true;}
        if(strcmp(argv[2],"fault")==0) {test_adc_fault=true;}
        if(strcmp(argv[2],"waitcharge")==0) {
            current_display={false,false,0U,1U,false};
            test_charging=true;
            test_ble_connected=false;
        }
        if(strcmp(argv[2],"max")==0) {current_display={true,true,30000U,6U,true};}
        if(strcmp(argv[2],"graph")==0) {
            // [新增] 建立測試曲線，不使用自製 SPI/畫面驅動。
            static const uint16_t sample[] = {3000,6200,9700,13000,17000,21800,25200,28000,
                29980,29000,27200,25900,23100,20500,17600,15000,13200,11200,9300,7000,4900};
            for (unsigned i=0;i<sizeof(sample)/sizeof(sample[0]);i++) {
                brd_ui::curve_observe({true,false,sample[i],1U,false},
                    millis() + (uint32_t)i*UI_CURVE_SAMPLE_MS);
            }
            brd_ui::curve_observe({true,true,30000U,2U,true}, millis()+2200U);
            current_display = {true,true,30000U,2U,true};
            test_button = true;
            brd_tft_update();
            delay(UI_BUTTON_DEBOUNCE_MS + 2U);
            brd_tft_update();
            assert(brd_ui::current_page() == brd_ui::Page::Graph);
        }
        if(!set_ota_mode(argv[2]) && strncmp(argv[2],"ir",2)==0) {
            test_page=BRD_DISPLAY_PAGE_IR_TEST;
            test_ir1=strcmp(argv[2],"ir1")==0 || strcmp(argv[2],"irboth")==0;
            test_ir2=strcmp(argv[2],"ir2")==0 || strcmp(argv[2],"irboth")==0;
            char voltage[12];
            const unsigned cv=(4125U+5U)/10U;
            snprintf(voltage,sizeof(voltage),"%u.%02uV",cv/100U,cv%100U);
            assert(strcmp(voltage,"4.13V")==0);
        }
        brd_tft_update();
    } /* 無額外頁面參數時保留 brd_tft_begin() 所畫的 Boot 畫面。 */
    FILE *f=fopen(argv[1],"wb");
    assert(f!=nullptr);
    fprintf(f,"P6\n160 80\n255\n");
    for(size_t i=0;i<TFT_WIDTH*TFT_HEIGHT;i++) {
        const uint16_t rgb565=brd_ui::lcd().pixels[i];
        const uint8_t r=static_cast<uint8_t>(((rgb565>>11)&31U)*255U/31U);
        const uint8_t g=static_cast<uint8_t>(((rgb565>>5)&63U)*255U/63U);
        const uint8_t b=static_cast<uint8_t>((rgb565&31U)*255U/31U);
        fputc(r,f); fputc(g,f); fputc(b,f);
    }
    fclose(f);
    printf("PASS TFT preview %s\n",argc==3?argv[2]:"boot");
    return 0;
}
