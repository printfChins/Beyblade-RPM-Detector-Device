/*
    檔案名稱: BLE_RPM_V1.0.ino
    檔案位置: BLE_RPM_V1.0/BLE_RPM_V1.0.ino
    專案名稱: Beyblade RPM Detector
    專案縮寫: BRD
    MCU: ESP32-C3
    Library: NimBLE-Arduino

    燒錄設置:
    開發板: "ESP32C3 Dev Module"
    Upload Speed: "921600"
    USB CDC On Boot: "Enabled"
    CPU Frequency: "40MHz"
    Flash Frequency: "80MHz"
    Flash Mode: "DIO"
    Flash Size: "4MB (32Mb)"
    Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
    Core Debug Level: "無"
    Erase All Flash Before Sketch Upload: "Disabled"
    JTAG Adapter: "Integrated USB JTAG"
    Zigbee Mode: "Disabled"

    setup() 與 loop() 只負責模組初始化及非阻塞任務排程。
    loop() 執行順序與原程式一致。
*/

#include <Arduino.h>

#include "brd_ble.h"
#include "brd_config.h"
#include "brd_context.h"
#include "brd_io.h"
#include "brd_log.h"
#include "brd_measurement.h"

void setup(void) {
    setCpuFrequencyMhz(CPU_FREQ_MHZ);

    Serial.begin(115200);
    delay(500);

    brd_context_reset();
    brd_ble_make_device_name();
    brd_log_print_startup();

    brd_io_begin();
    brd_measurement_begin();
    brd_status_led_update_task();

    brd_ble_init();
    brd_log_print_ready();
}

void loop(void) {
    /*
        1. GPIO1 裝載狀態優先更新。
    */
    brd_load_poll_update();

    /*
        2. GPIO0 RPM IR 輪巡。
    */
    brd_rpm_poll_update();

    /*
        3. 更新充電狀態。
    */
    brd_charge_update_task();

    /*
        4. 長時間無脈衝時將即時 RPM 歸零。
    */
    brd_rpm_zero_update_task();

    /*
        5. 固定週期記錄 RPM 曲線。
    */
    brd_curve_record_update_task();

    /*
        6. 發射後判斷停止條件。
    */
    brd_measurement_stop_update_task();

    /*
        7. GPIO8 LED 模式仲裁。
    */
    brd_status_led_update_task();

    /*
        8. BLE 即時狀態與 RPM。
    */
    brd_ble_send_live_task();

    /*
        9. 傳送發射事件。
    */
    brd_ble_send_launch_event_task();

    /*
        10. 傳送完整量測結果。
    */
    brd_ble_send_result_task();

    /*
        11. Serial Runtime LOG。
    */
    brd_log_update_task();

    yield();
}
