/* [新增] RPM 優先：執行真正量測、設定與 Session，僅替代 MCU 邊界。 */
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <Preferences.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
std::map<std::string, std::vector<uint8_t>> test_nvs;
unsigned test_flash_writes = 0U;
bool test_flash_fail = false;
static brd_bbp::Session session;
bool brd_battery_measurement_allowed() { return true; }
bool brd_io_is_charging() { return false; }
void brd_bbp_capture_reset() { session.reset_capture(); }
void brd_bbp_capture_period(uint32_t p, uint32_t t, bool r) { session.period(p,t,r); }
void brd_bbp_capture_launch(uint32_t t) { session.launched(t); }
void brd_bbp_capture_finish() { session.finish(); }
void brd_bbp_capture_abort() { session.abort(); }
static void tick(uint32_t us) { test_clock_us += us; brd_measurement_update(); }
static void edge(uint32_t us, int level=HIGH) {
    test_clock_us += us;
    GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) | (static_cast<uint32_t>(level) << RPM_IR_GPIO);
    if (test_interrupts[RPM_IR_GPIO]) { test_interrupts[RPM_IR_GPIO](); }
}
static void load(int level) {
    GPIO.in.val = (GPIO.in.val & ~(1U << LOAD_IR_GPIO)) | (static_cast<uint32_t>(level) << LOAD_IR_GPIO);
    if (test_interrupts[LOAD_IR_GPIO]) { test_interrupts[LOAD_IR_GPIO](); }
    brd_measurement_update(); tick(51000U);
}
static brd_settings::Status command(uint8_t op,uint8_t param=0U,uint32_t value=0U) {
    uint8_t data[8]={0xC1,1,op,9,param,static_cast<uint8_t>(value),static_cast<uint8_t>(value>>8),static_cast<uint8_t>(value>>16)};
    uint8_t reply[20]; return brd_settings_process_request(data,sizeof(data),reply);
}
static void ready() {
    brd_settings_begin(); brd_measurement_begin(); load(HIGH);
}
int main(int argc, char **argv) {
    assert(argc==2); const int which=std::atoi(argv[1]);
    if (which==1) {
        ready();
        assert(brd_measurement_pause_for_settings());
        edge(100U); edge(5000U);
        brd_measurement_resume_after_settings(false);
        brd_measurement_update();
        assert(brd_measurement_get_display().value==12000U);
        puts("PASS: Flash transaction preserves incoming RPM events.");
    } else if (which==2) {
        ready(); edge(100U); edge(5000U); brd_measurement_update();
        edge(20000U); brd_measurement_update();
        auto prior=brd_measurement_get_display(); assert(prior.show_max && prior.value==12000U);
        brd_measurement_max_frame_presented(prior.generation);
        load(LOW); load(HIGH);
        edge(100U); edge(7500U); brd_measurement_update();
        assert(!brd_measurement_get_display().show_max && brd_measurement_get_display().value==8000U);
        puts("PASS: new LOAD measurement preempts OLED HOLD.");
    } else if (which==3) {
        session.period(5000U,100000U); session.launched(120000U); session.finish();
        session.reset_capture(); session.period(7500U,300000U);
        session.update(700000U,600000U);
        assert(session.state().total==1U && session.state().records[0]==12000U);
        session.launched(310000U); session.finish(); session.update(900000U,600000U);
        assert(session.state().total==2U && session.state().records[1]==8000U);
        puts("PASS: delayed completed shot survives new active capture.");
    } else if (which==4) {
        ready(); tick(1000000U);
        assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50)==brd_settings::OK);
        assert(command(brd_settings::CMD_SAVE)==brd_settings::PENDING);
        test_nvs_write_hook=[]() { edge(100U); edge(5000U); };
        brd_settings_update();
        assert(brd_settings_get().finish_percent==30U && brd_settings_model().pending());
        brd_measurement_update(); assert(brd_measurement_get_display().value==12000U);
        brd_settings_update(); assert(brd_settings_model().pending());
        edge(20000U); brd_measurement_update(); brd_settings_update();
        assert(brd_settings_get().finish_percent==50U && !brd_settings_model().pending());
        puts("PASS: NVS completion waits for real input processing before applying new settings.");
    } else if (which==5) {
        ready(); assert(brd_measurement_pause_for_ota());
        edge(100U); edge(5000U); brd_measurement_begin(); brd_measurement_update();
        assert(brd_measurement_get_display().value==0U);
        brd_measurement_resume_after_ota();
        edge(100U); edge(5000U); brd_measurement_update();
        assert(brd_measurement_get_display().value==12000U);
        puts("PASS: explicit OTA holds capture until release; normal begin cannot override it.");
    } else if (which==6) {
        test_gpio_install_result=1; brd_settings_begin(); brd_measurement_begin();
        assert(brd_measurement_get_display().input_fault && test_interrupts[RPM_IR_GPIO]==nullptr);
        test_gpio_install_result=ESP_OK; brd_measurement_begin(); load(HIGH);
        edge(100U); edge(5000U); brd_measurement_update();
        assert(!brd_measurement_get_display().input_fault && brd_measurement_get_display().value==12000U);
        assert(test_gpio_install_flags==ESP_INTR_FLAG_IRAM);
        auto diag=brd_measurement_get_diagnostics();
        assert(diag.capture_init_failures==1U && diag.rpm_events_enqueued==2U && diag.rpm_isr_entries==2U);
        puts("PASS: native capture fails visibly, recovers, and reports ISR health.");
    } else if (which==7) {
        ready(); edge(100U); edge(2000U); brd_measurement_update();
        for (unsigned i=0;i<RPM_ISR_QUEUE_SIZE+30U;i++) { edge(2000U); }
        brd_measurement_update(); edge(2000U); brd_measurement_update(); edge(2000U); brd_measurement_update();
        assert(brd_measurement_get_display().value==30000U);
        edge(20000U); brd_measurement_update(); session.update(micros()+600000U,600000U);
        assert(session.state().total==0U && !brd_measurement_get_display().show_max);
        puts("PASS: known event loss never publishes an unverified MAX/history result.");
    } else if (which==8) {
        ready(); tick(1000000U);
        assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50)==brd_settings::OK);
        assert(command(brd_settings::CMD_SAVE)==brd_settings::PENDING);
        test_nvs_write_hook=[]() { edge(100U); edge(5000U); };
        brd_settings_update(); assert(brd_settings_model().pending());
        brd_measurement_stop(); brd_measurement_begin();
        brd_measurement_update(); tick(51000U);
        brd_settings_update(); brd_settings_update();
        assert(!brd_settings_model().pending() && brd_settings_get().finish_percent==50U);
        puts("PASS: ADC-style measurement restart cannot strand a saved settings transaction.");
    } else if (which==9) {
        ready(); tick(1000000U); edge(100U); edge(5000U); brd_measurement_update();
        edge(20000U); brd_measurement_update();
        assert(command(brd_settings::CMD_SET,brd_settings::PARAM_MODE,1U)==brd_settings::OK);
        assert(command(brd_settings::CMD_SAVE)==brd_settings::PENDING); brd_settings_update();
        for(unsigned i=0;i<20;i++) { edge(20000U); brd_measurement_update(); }
        tick(310000U); session.update(micros()+600000U,600000U);
        assert(brd_measurement_get_diagnostics().launch_events==1U && session.state().total==1U);
        puts("PASS: applying AUTO after completion cannot count the same spinning tail twice.");
    } else if (which==10) {
        ready();
        for (unsigned mode=0U;mode<2U;mode++) {
            if(mode==1U) {
                assert(command(brd_settings::CMD_SET,brd_settings::PARAM_MODE,1U)==brd_settings::OK);
                assert(command(brd_settings::CMD_SAVE)==brd_settings::PENDING); brd_settings_update();
            }
            uint32_t initial_total=session.state().total;
            for(unsigned shot=0U;shot<7200U;shot++) {
                uint64_t start=test_clock_us;
                if(mode==0U) { load(LOW); load(HIGH); }
                edge(100U,HIGH); brd_measurement_update();
                for(unsigned revolution=0U;revolution<40U;revolution++) {
                    edge(2500U,LOW); brd_measurement_update();
                    edge(2500U,HIGH); brd_measurement_update();
                    assert(brd_measurement_get_display().value==12000U);
                }
                edge(1000U,LOW); brd_measurement_update(); edge(1000U,HIGH); brd_measurement_update();
                assert(brd_measurement_get_display().value==30000U);
                edge(10000U,LOW); brd_measurement_update(); edge(10000U,HIGH); brd_measurement_update();
                auto display=brd_measurement_get_display();
                assert(display.show_max && display.value==30000U && !display.sample_loss);
                brd_measurement_max_frame_presented(display.generation);
                while(test_clock_us-start<1000000ULL) {
                    uint32_t step=static_cast<uint32_t>(1000000ULL-(test_clock_us-start));
                    if(step>1000U) { step=1000U; }
                    tick(step); session.update(micros(),600000U);
                }
                assert(session.state().total==initial_total+shot+1U);
                assert(session.state().lifetime_max==30000U);
                for(unsigned i=0;i<32U;i++) { assert(session.state().curve[i]==625U); }
            }
            std::printf("PASS: mode=%u, 120 virtual minutes, 7200 valid shots, MAX 30000, first 32 periods intact.\n",mode);
        }
        auto diag=brd_measurement_get_diagnostics();
        assert(diag.rpm_queue_overflows==0U && diag.rpm_capture_disabled==0U && diag.invalid_measurements==0U);
        assert(diag.launch_events==14400U);
        puts("PASS: four-hour combined run crosses signed and unsigned micros boundaries without capture loss.");
    } else if (which==11) {
        ready(); tick(1000000U);
        assert(command(brd_settings::CMD_SET,brd_settings::PARAM_FINISH_PERCENT,50U)==brd_settings::OK);
        assert(command(brd_settings::CMD_SAVE)==brd_settings::PENDING); brd_settings_update();
        edge(100U); edge(5000U); brd_measurement_update();
        assert(brd_measurement_get_display().value==12000U);
        puts("PASS: same-mode save preserves established LOAD readiness and first RPM.");
    } else if (which==12) {
        ready(); edge(100U); edge(5000U); brd_measurement_update(); edge(20000U); brd_measurement_update();
        for(unsigned i=0;i<2000U;i++) { edge(2000U); }
        brd_measurement_update();
        edge(2000U); brd_measurement_update(); edge(2000U); brd_measurement_update(); edge(20000U); brd_measurement_update();
        session.update(micros()+600000U,600000U);
        assert(brd_measurement_get_diagnostics().launch_events==1U && session.state().total==1U);
        tick(310000U); edge(100U); brd_measurement_update(); edge(7500U); brd_measurement_update();
        assert(brd_measurement_get_display().value==8000U && !brd_measurement_get_display().show_max);
        puts("PASS: overflowing tail cannot fake quiet, duplicate shots, or prevent real rearm.");
    } else if (which==13) {
        session.period(5000U,100000U); session.launched(120000U); session.finish();
        assert((session.flags(80U,20U,5U)&4U)!=0U);
        session.reset_capture(); assert((session.flags(80U,20U,5U)&4U)!=0U);
        session.update(700000U,600000U); assert((session.flags(80U,20U,5U)&4U)==0U);
        puts("PASS: strict BBP status retains pending-result flag until actual publication.");
    }
}
