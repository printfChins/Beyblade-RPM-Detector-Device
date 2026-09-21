/*
    檔案位置: BRD_BBP/brd_bbp.cpp
    [BRD_BBP 新增] 完整新檔，NimBLE-Arduino 2.5.1。
    callback 只入列；主 loop 建立一致快照，逐頁通知。ISR 不寫 BLE 或 NVS。
    [BRD_BBP 修正 新增] 支援只訂閱通知的上位機，補上訂閱、LOAD 與結果事件。
    [刪減] 預設不再依賴上位機一定會寫入 0x51 / 0x74。
*/
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_bbp_session.h"
#include "brd_config.h"
#include "brd_measurement.h"

struct bbp_command_t {
    uint32_t generation;
    uint16_t handle;
    uint8_t command;
};
struct bbp_link_t {
    uint32_t generation;
    uint16_t handle;
    bool connected, subscribed, overflow;
};
static portMUX_TYPE g_link_mux = portMUX_INITIALIZER_UNLOCKED;
static bbp_link_t g_link = {0U, BLE_HS_CONN_HANDLE_NONE, false, false, false};
static brd_bbp_diagnostics_t g_diagnostics = {};
static StaticQueue_t g_command_queue_control;
static uint8_t g_command_queue_storage[BBP_COMMAND_QUEUE_SIZE * sizeof(bbp_command_t)];
static QueueHandle_t g_command_queue = nullptr;
static NimBLEServer *g_server = nullptr;
static NimBLECharacteristic *g_notify_characteristic = nullptr;
static NimBLECharacteristic *g_write_characteristic = nullptr;
static bool g_started = false;
static bool g_accept_connections = false;
static bool g_storage_loaded = false;
static uint32_t g_last_start_attempt_ms = 0U;
static bool g_start_attempted = false;
static uint32_t g_last_advertise_attempt_ms = 0U;
static uint32_t g_last_storage_attempt_ms = 0U;
/* [BRD_BBPX 保留] Device UID 直接使用 ESP32-C3 eFuse default MAC 6 bytes。 */
static uint8_t g_uid[6] = {};
/* [BRD_BBPX 新增] BRD-XXXX，XXXX 取 eFuse MAC 最後 2 bytes。 */
static char g_device_name[16] = {};
static brd_bbp::Session g_session;
static uint8_t g_storage[brd_bbp::STORAGE_SIZE];
static uint8_t g_tx_pages[brd_bbp::PAGE_COUNT][brd_bbp::PACKET_SIZE];
static uint8_t g_tx_count = 0U, g_tx_index = 0U;
static uint32_t g_tx_generation = 0U, g_last_tx_ms = 0U, g_page_started_ms = 0U;
/* [修正 新增] 主動事件只由 loop 排程，與命令回覆共用同一組傳輸快照。 */
static uint32_t g_link_generation_seen = UINT32_MAX;
static uint32_t g_subscribe_ms = 0U;
static bool g_auto_a0_pending = false, g_auto_dump_pending = false;
static bool g_last_loaded = false;

static void increment_diagnostic(uint32_t &counter) {
    portENTER_CRITICAL(&g_link_mux);
    if (counter != UINT32_MAX) {
        counter++;
    }
    portEXIT_CRITICAL(&g_link_mux);
}
static bbp_link_t link_snapshot(void) {
    portENTER_CRITICAL(&g_link_mux);
    bbp_link_t value = g_link;
    portEXIT_CRITICAL(&g_link_mux);
    return value;
}
class BbpServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
        bool reject;
        portENTER_CRITICAL(&g_link_mux);
        reject = !g_accept_connections || g_link.connected;
        if (!reject) {
            g_link.generation++;
            g_link.handle = info.getConnHandle();
            g_link.connected = true;
            g_link.subscribed = false;
            g_link.overflow = false;
        }
        portEXIT_CRITICAL(&g_link_mux);
        if (reject) {
            server->disconnect(info.getConnHandle());
        }
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &info, int) override {
        portENTER_CRITICAL(&g_link_mux);
        if (g_link.handle == info.getConnHandle()) {
            g_link.generation++;
            g_link.handle = BLE_HS_CONN_HANDLE_NONE;
            g_link.connected = g_link.subscribed = g_link.overflow = false;
        }
        portEXIT_CRITICAL(&g_link_mux);
    }
};
class BbpCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &info, uint16_t value) override {
        portENTER_CRITICAL(&g_link_mux);
        if (g_link.connected && g_link.handle == info.getConnHandle()) {
            bool subscribed = (value & 1U) != 0U;
            if (g_link.subscribed != subscribed) {
                /* [新增] 世代隔離避免新訂閱收到舊傳輸後半段。 */
                g_link.generation++;
                g_link.subscribed = subscribed;
            }
        }
        portEXIT_CRITICAL(&g_link_mux);
    }
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        const auto value = characteristic->getValue();
        if (value.size() != 1U) {
            increment_diagnostic(g_diagnostics.invalid_commands);
            return;
        }
        bbp_link_t link = link_snapshot();
        if (!link.connected || link.handle != info.getConnHandle()) {
            return;
        }
        uint8_t command = value[0];
        if (command != 0x51U && command != 0x74U && command != 0x75U && command != 0x61U) {
            increment_diagnostic(g_diagnostics.unknown_commands);
            return;
        }
        if ((command == 0x51U || command == 0x74U) && !link.subscribed) {
            increment_diagnostic(g_diagnostics.invalid_commands);
            return;
        }
        bbp_command_t request = {link.generation, link.handle, command};
        if (xQueueSend(g_command_queue, &request, 0) != pdTRUE) {
            /* [新增] 不合併或覆寫輪詢命令，超出有界佇列則斷線重讀。 */
            portENTER_CRITICAL(&g_link_mux);
            if (g_link.generation == link.generation) {
                g_link.overflow = true;
            }
            portEXIT_CRITICAL(&g_link_mux);
            increment_diagnostic(g_diagnostics.command_queue_overflows);
        }
    }
};
static BbpServerCallbacks g_server_callbacks;
static BbpCharacteristicCallbacks g_characteristic_callbacks;

static bool load_storage(void) {
    if (g_storage_loaded) {
        return true;
    }
    Preferences preferences;
    if (!preferences.begin(BBP_NVS_NAMESPACE, false)) {
        increment_diagnostic(g_diagnostics.storage_failures);
        return false;
    }
    if (preferences.isKey(BBP_NVS_KEY)) {
        brd_bbp::State restored;
        size_t length = preferences.getBytesLength(BBP_NVS_KEY);
        if (length != sizeof(g_storage) ||
            preferences.getBytes(BBP_NVS_KEY, g_storage, sizeof(g_storage)) != sizeof(g_storage) ||
            !brd_bbp::decode_storage(g_storage, sizeof(g_storage), restored)) {
            /* [修正] 未能確認舊歷史時禁止覆寫，包括短讀、CRC 或版本錯誤。 */
            increment_diagnostic(g_diagnostics.storage_failures);
            preferences.end();
            return false;
        }
        g_session.restore(restored);
    }
    preferences.end();
    /* [修正] 成功讀取或確認新 namespace 後才標記 ready。 */
    g_storage_loaded = true;
    return true;
}
static void save_storage(uint32_t now_ms, bool allowed) {
    if (!g_storage_loaded || !allowed || !g_session.dirty() ||
        static_cast<uint32_t>(now_ms - g_last_storage_attempt_ms) < BBP_RETRY_INTERVAL_MS) {
        return;
    }
    g_last_storage_attempt_ms = now_ms;
    brd_bbp::encode_storage(g_session.state(), g_storage);
    Preferences preferences;
    if (!preferences.begin(BBP_NVS_NAMESPACE, false)) {
        increment_diagnostic(g_diagnostics.storage_failures);
        return;
    }
    /* [新增] 單一版本化 blob，NVS commit 加上 CRC，不儲存 struct padding。 */
    if (preferences.putBytes(BBP_NVS_KEY, g_storage, sizeof(g_storage)) == sizeof(g_storage)) {
        g_session.mark_saved();
    } else {
        increment_diagnostic(g_diagnostics.storage_failures);
    }
    preferences.end();
}
bool brd_bbp_begin(void) {
    if (g_started) {
        return true;
    }
    if (!brd_battery_measurement_allowed()) {
        return false;
    }
    uint32_t now_ms = millis();
    if (g_start_attempted &&
        static_cast<uint32_t>(now_ms - g_last_start_attempt_ms) < BBP_RETRY_INTERVAL_MS) {
        return false;
    }
    g_start_attempted = true;
    g_last_start_attempt_ms = now_ms;
    if (!load_storage()) {
        return false;
    }
    if (g_command_queue == nullptr) {
        g_command_queue = xQueueCreateStatic(BBP_COMMAND_QUEUE_SIZE, sizeof(bbp_command_t),
                                             g_command_queue_storage, &g_command_queue_control);
    }
    if (g_command_queue == nullptr || esp_efuse_mac_get_default(g_uid) != ESP_OK) {
        increment_diagnostic(g_diagnostics.init_failures);
        return false;
    }
    /* [BRD_BBPX 新增] BLE Device Name = BRD-XXXX，與封包 Device UID 使用同一顆 eFuse MAC。 */
    int device_name_length = snprintf(g_device_name, sizeof(g_device_name), "%s%02X%02X",
                                      BBPX_DEVICE_NAME_PREFIX, g_uid[4], g_uid[5]);
    if (device_name_length <= 0 || static_cast<size_t>(device_name_length) >= sizeof(g_device_name)) {
        increment_diagnostic(g_diagnostics.init_failures);
        return false;
    }
    xQueueReset(g_command_queue);
    if (!NimBLEDevice::init(g_device_name)) {
        increment_diagnostic(g_diagnostics.init_failures);
        brd_bbp_stop();
        return false;
    }
    bool success = NimBLEDevice::setPower(BBP_TX_POWER_DBM);
    g_server = NimBLEDevice::createServer();
    if (g_server != nullptr) {
        g_server->setCallbacks(&g_server_callbacks, false);
        g_server->advertiseOnDisconnect(false);
        NimBLEService *service = g_server->createService(BBPX_SERVICE_UUID);
        if (service != nullptr) {
            /* [BRD_BBPX 修改] 原單一雙向 Characteristic 拆成 BRD Notify / Write 兩個 UUID。 */
            g_notify_characteristic = service->createCharacteristic(BBPX_NOTIFY_UUID,
                NIMBLE_PROPERTY::NOTIFY, brd_bbp::PACKET_SIZE);
            g_write_characteristic = service->createCharacteristic(BBPX_WRITE_UUID,
                NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, brd_bbp::PACKET_SIZE);
        }
        if (g_notify_characteristic != nullptr && g_write_characteristic != nullptr) {
            g_notify_characteristic->setCallbacks(&g_characteristic_callbacks);
            g_write_characteristic->setCallbacks(&g_characteristic_callbacks);
            success = g_server->start() && success;
        } else {
            success = false;
        }
    } else {
        success = false;
    }
    NimBLEAdvertisementData advertisement;
    NimBLEAdvertisementData scan_response;
    success = advertisement.setFlags(0x06U) && success;
    /* [BRD_BBPX 修改] Advertising payload 只放 BRD Service；名稱只放 Scan Response。 */
    success = advertisement.addServiceUUID(BBPX_SERVICE_UUID) && success;
    success = scan_response.setName(g_device_name) && success;
    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    if (advertising != nullptr) {
        advertising->enableScanResponse(true);
        success = advertising->setAdvertisementData(advertisement) && success;
        success = advertising->setScanResponseData(scan_response) && success;
    } else {
        success = false;
    }
    portENTER_CRITICAL(&g_link_mux);
    g_accept_connections = success;
    portEXIT_CRITICAL(&g_link_mux);
    if (!success || !advertising->start()) {
        increment_diagnostic(g_diagnostics.init_failures);
        brd_bbp_stop();
        return false;
    }
    g_started = true;
    g_last_advertise_attempt_ms = now_ms;
    g_last_tx_ms = now_ms - BBP_NOTIFY_INTERVAL_MS;
    return true;
}
void brd_bbp_stop(void) {
    portENTER_CRITICAL(&g_link_mux);
    g_accept_connections = false;
    g_link.generation++;
    g_link.handle = BLE_HS_CONN_HANDLE_NONE;
    g_link.connected = g_link.subscribed = g_link.overflow = false;
    portEXIT_CRITICAL(&g_link_mux);
    g_tx_count = g_tx_index = 0U;
    g_auto_a0_pending = g_auto_dump_pending = false;
    g_link_generation_seen = UINT32_MAX;
    if (g_command_queue != nullptr) {
        xQueueReset(g_command_queue);
    }
    if (NimBLEDevice::isInitialized()) {
        NimBLEDevice::deinit(true);
    }
    g_started = false;
    g_server = nullptr;
    g_notify_characteristic = nullptr;
    g_write_characteristic = nullptr;
}
/*
    [修正 新增] 命令與主動通知使用同一個封包建立入口。
    相容模式只以 LOAD 表示 A0 狀態，純輪詢模式保留文件的電量及轉動旗標。
*/
static void prepare_response(uint8_t command, const bbp_link_t &link, uint32_t now_ms) {
    uint8_t percent = brd_battery_get_percent();
    uint8_t raw = static_cast<uint8_t>((static_cast<uint16_t>(percent) * 250U + 50U) / 100U);
    uint8_t flags = BBP_COMPAT_AUTONOTIFY ? (g_last_loaded ? 0x04U : 0U) :
        g_session.flags(percent, BBP_BATTERY_WARNING_PERCENT, BBP_BATTERY_CRITICAL_PERCENT);
    g_tx_count = g_session.command(command, raw, g_uid, flags, g_tx_pages);
    g_tx_index = 0U;
    g_tx_generation = link.generation;
    g_page_started_ms = now_ms;
    if (command == 0x51U) {
        g_auto_a0_pending = false;
    } else if (command == 0x74U) {
        g_auto_dump_pending = false;
    } else if (command == 0x75U) {
        /* [修正 新增] 清除不能讓待送的舊結果在下一輪再次出現。 */
        g_auto_dump_pending = false;
    }
}
void brd_bbp_update(bool storage_allowed) {
    if (!g_storage_loaded) {
        return;
    }
    uint32_t now_ms = millis();
    /* [新增] 沒有連線仍完成本地紀錄，發布不依賴手機輪詢。 */
    uint32_t previous_total = g_session.state().total;
    g_session.update(micros(), BBP_PUBLISH_MIN_DELAY_US);
    bool published = previous_total != g_session.state().total;
    save_storage(now_ms, storage_allowed);
    if (!g_started) {
        return;
    }
    bbp_link_t link = link_snapshot();
    if (BBP_COMPAT_AUTONOTIFY) {
        bool loaded = brd_measurement_get_display().loaded;
        if (link.generation != g_link_generation_seen) {
            g_link_generation_seen = link.generation;
            g_subscribe_ms = now_ms;
            g_auto_a0_pending = link.connected && link.subscribed;
            g_auto_dump_pending = false;
            g_last_loaded = loaded;
        }
        if (link.connected && link.subscribed) {
            if (loaded != g_last_loaded) {
                g_auto_a0_pending = true;
            }
            if (published) {
                g_auto_dump_pending = true;
            }
        }
        g_last_loaded = loaded;
    }
    if (g_tx_count != 0U &&
        (!link.connected || !link.subscribed || link.generation != g_tx_generation)) {
        g_tx_count = g_tx_index = 0U;
    }
    if (!link.connected &&
        static_cast<uint32_t>(now_ms - g_last_advertise_attempt_ms) >= BBP_RETRY_INTERVAL_MS) {
        g_last_advertise_attempt_ms = now_ms;
        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        if (!advertising->isAdvertising() && !advertising->start()) {
            increment_diagnostic(g_diagnostics.init_failures);
        }
    }
    if (link.overflow) {
        g_tx_count = 0U;
        xQueueReset(g_command_queue);
        if (link.connected) {
            g_server->disconnect(link.handle);
        }
        return;
    }
    if (g_tx_count == 0U) {
        bbp_command_t request;
        if (xQueueReceive(g_command_queue, &request, 0) == pdTRUE) {
            if (!link.connected || request.generation != link.generation || request.handle != link.handle) {
                return;
            }
            prepare_response(request.command, link, now_ms);
            if (request.command == 0x75U) {
                g_last_storage_attempt_ms = now_ms - BBP_RETRY_INTERVAL_MS;
                save_storage(now_ms, storage_allowed);
            }
        }
    }
    /*
        [修正 新增] 無命令也能建立通知；12 頁未送完時不插入 A0 或新資料。
        訂閱後等待 100 ms，避免上位機的接收處理尚未準備好。
    */
    if (BBP_COMPAT_AUTONOTIFY && link.connected && link.subscribed &&
        static_cast<uint32_t>(now_ms - g_subscribe_ms) < BBP_SUBSCRIBE_SETTLE_MS) {
        return;
    }
    if (BBP_COMPAT_AUTONOTIFY && g_tx_count == 0U && link.connected && link.subscribed) {
        if (g_auto_a0_pending) {
            prepare_response(0x51U, link, now_ms);
        } else if (g_auto_dump_pending) {
            prepare_response(0x74U, link, now_ms);
        }
    }
    if (g_tx_count == 0U ||
        static_cast<uint32_t>(now_ms - g_last_tx_ms) < BBP_NOTIFY_INTERVAL_MS) {
        return;
    }
    link = link_snapshot();
    if (!link.connected || !link.subscribed || link.generation != g_tx_generation) {
        g_tx_count = 0U;
        return;
    }
    g_last_tx_ms = now_ms;
    if (g_notify_characteristic->notify(g_tx_pages[g_tx_index], brd_bbp::PACKET_SIZE, link.handle)) {
        /* [新增] true 只是交給 BLE stack，不代表應用層 ACK。 */
        g_tx_index++;
        g_page_started_ms = now_ms;
        if (g_tx_index == g_tx_count) {
            g_tx_count = 0U;
        }
    } else {
        increment_diagnostic(g_diagnostics.notify_failures);
        if (static_cast<uint32_t>(now_ms - g_page_started_ms) >= BBP_NOTIFY_TIMEOUT_MS) {
            g_tx_count = 0U;
            g_server->disconnect(link.handle);
        }
    }
}
bool brd_bbp_is_connected(void) {
    return link_snapshot().connected;
}
bool brd_bbp_prepare_restart(void) {
    /* [修正] 呼叫端已確認 >=10% 持續恢復穩定，才允許寫入並重啟。 */
    if (!g_storage_loaded || !g_session.dirty()) {
        return true;
    }
    save_storage(millis(), true);
    return !g_session.dirty();
}
brd_bbp_diagnostics_t brd_bbp_get_diagnostics(void) {
    portENTER_CRITICAL(&g_link_mux);
    brd_bbp_diagnostics_t result = g_diagnostics;
    portEXIT_CRITICAL(&g_link_mux);
    return result;
}
void brd_bbp_capture_reset(void) {
    if (g_storage_loaded) {
        g_session.reset_capture();
    }
}
// [R3 修改] 將本次參考邊沿的曲線寫入選擇傳遞給 Session。
void brd_bbp_capture_period(uint32_t period_us, uint32_t event_us, bool record_profile) {
    if (g_storage_loaded) {
        g_session.period(period_us, event_us, record_profile);
    }
}
// [R2 修改] 傳遞發射標記，Session 不清除裝載後的拉轉曲線。
void brd_bbp_capture_launch(uint32_t launch_us) {
    if (g_storage_loaded) {
        g_session.launched(launch_us);
    }
}
void brd_bbp_capture_finish(void) {
    if (g_storage_loaded) {
        g_session.finish();
    }
}
void brd_bbp_capture_abort(void) {
    if (g_storage_loaded) {
        g_session.abort();
    }
}
