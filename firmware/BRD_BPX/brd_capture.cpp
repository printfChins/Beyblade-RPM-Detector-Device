/*
    [新增] 檔案位置：BRD_BPX/brd_capture.cpp，與主程式放同一資料夾。
    [刪減替代] 不使用 Arduino attachInterrupt 的非 IRAM wrapper。
    RPM/LOAD callback、時間函式與資料均使用內部 RAM 路徑。
*/
#include <driver/gpio.h>
#include <esp_intr_alloc.h>
#include <esp_timer.h>
#include "brd_config.h"
#include "brd_capture.h"
static bool g_ready = false;
static brd_capture_handler_t g_rpm_handler = nullptr;
static brd_capture_handler_t g_load_handler = nullptr;
static void IRAM_ATTR rpm_dispatch(void *) {
    if (g_rpm_handler != nullptr) { g_rpm_handler(); }
}
static void IRAM_ATTR load_dispatch(void *) {
    if (g_load_handler != nullptr) { g_load_handler(); }
}
bool brd_capture_prepare(void) {
    if (g_ready) { return true; }
    /* 已由其他模組安裝的 service 不能假設具備 IRAM 屬性；失敗須回報。 */
    if (gpio_install_isr_service(ESP_INTR_FLAG_IRAM) != ESP_OK) { return false; }
    g_ready = true;
    return true;
}
static bool attach(gpio_num_t pin, gpio_isr_t dispatch) {
    if (!brd_capture_prepare()) { return false; }
    if (gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE) != ESP_OK) { return false; }
    if (gpio_isr_handler_add(pin, dispatch, nullptr) != ESP_OK) { return false; }
    if (gpio_intr_enable(pin) == ESP_OK) { return true; }
    (void)gpio_isr_handler_remove(pin);
    return false;
}
bool brd_capture_attach_rpm(brd_capture_handler_t handler) {
    g_rpm_handler = handler;
    return attach(static_cast<gpio_num_t>(RPM_IR_GPIO), rpm_dispatch);
}
bool brd_capture_attach_load(brd_capture_handler_t handler) {
    g_load_handler = handler;
    return attach(static_cast<gpio_num_t>(LOAD_IR_GPIO), load_dispatch);
}
void brd_capture_detach_rpm(void) {
    (void)gpio_intr_disable(static_cast<gpio_num_t>(RPM_IR_GPIO));
    (void)gpio_isr_handler_remove(static_cast<gpio_num_t>(RPM_IR_GPIO));
    g_rpm_handler = nullptr;
}
void brd_capture_detach_load(void) {
    (void)gpio_intr_disable(static_cast<gpio_num_t>(LOAD_IR_GPIO));
    (void)gpio_isr_handler_remove(static_cast<gpio_num_t>(LOAD_IR_GPIO));
    g_load_handler = nullptr;
}
uint32_t IRAM_ATTR brd_capture_time_us(void) {
    return static_cast<uint32_t>(esp_timer_get_time());
}
