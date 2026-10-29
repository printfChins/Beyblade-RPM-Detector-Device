/*
    檔案位置: BRD_BPX/brd_ota.cpp
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [V1.2 新增] Callback 只入列；Flash、MD5、分區切換均由主 loop 處理。
    [V1.2 新增] OTA 持有量測 pause，失敗釋放，成功保持直到重新啟動。
*/
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/md5.h>
#include "brd_ota.h"
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_scheduler.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"
#include "brd_settings.h"

static bool g_verifying = false;
static uint32_t current_connection_generation();
static bool generation_connected(uint32_t generation);
static bool claim_commit(uint32_t generation);
class OtaFlash : public brd_ota::Sink {
    const esp_partition_t *partition_ = nullptr;
    esp_ota_handle_t handle_ = 0U;
    uint32_t size_ = 0U;
    uint32_t generation_ = UINT32_MAX;
    bool opened_ = false, paused_ = false, hash_open_ = false;
    uint8_t expected_[16] = {};
    /* [V1.2 新增] 分塊寫 Flash，避免每個 BLE 封包都呼叫 esp_ota_write。 */
    uint8_t page_[4096] = {};
    size_t page_used_ = 0U;
    bool flush_page() {
        if (page_used_ == 0U) { return true; }
        if (!opened_ || esp_ota_write(handle_, page_, page_used_) != ESP_OK) { return false; }
        page_used_ = 0U;
        return true;
    }
    mbedtls_md5_context hash_;
public:
    OtaFlash() { mbedtls_md5_init(&hash_); }
    uint32_t capacity() const override {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
        if (running == nullptr || next == nullptr || running->address == next->address ||
            next->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
            next->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_15) { return 0U; }
        return next->size;
    }
    bool begin(uint32_t size, const uint8_t md5[16]) override {
        /* [新增] 先完成歷史保存，再原子取得空 GPIO 佇列的量測安全窗口。 */
        if (capacity() < size || !brd_bbp_ota_ready() || !brd_bbp_prepare_restart() ||
            !brd_measurement_pause_for_ota()) { return false; }
        paused_ = true;
        generation_ = current_connection_generation();
        if (!generation_connected(generation_)) { return false; }
        partition_ = esp_ota_get_next_update_partition(nullptr);
        if (esp_ota_begin(partition_, size, &handle_) != ESP_OK) { return false; }
        opened_ = true;
        page_used_ = 0U;
        size_ = size;
        memcpy(expected_, md5, sizeof(expected_));
        hash_open_ = true;
        return mbedtls_md5_starts(&hash_) == 0;
    }
    bool write(const uint8_t *data, size_t length) override {
        if (!opened_ || data == nullptr || mbedtls_md5_update(&hash_, data, length) != 0) { return false; }
        while (length != 0U) {
            size_t amount = sizeof(page_) - page_used_;
            if (amount > length) { amount = length; }
            memcpy(page_ + page_used_, data, amount);
            page_used_ += amount; data += amount; length -= amount;
            if (page_used_ == sizeof(page_) && !flush_page()) { return false; }
        }
        return true;
    }
    bool finish() override {
        g_verifying = true;
        /* [新增] 暫停量測期間送完整 VERIFY 畫面，再執行映像驗證。 */
        for (unsigned i = 0; i < 80U; i++) { brd_oled_update(); }
        uint8_t actual[16];
        /* [V1.2 新增] END 強制寫入最後不足 4 KiB 的尾塊。 */
        bool verified = opened_ && flush_page() && mbedtls_md5_finish(&hash_, actual) == 0 &&
                        memcmp(actual, expected_, sizeof(actual)) == 0;
        if (verified) {
            /* [新增] esp_ota_end 驗證 Flash 映像與晶片；end 後 handle 不再有效。 */
            esp_err_t result = esp_ota_end(handle_); opened_ = false;
            verified = result == ESP_OK;
        }
        if (hash_open_) { mbedtls_md5_free(&hash_); hash_open_ = false; }
        if (verified) {
            /* [修正 新增] 讀回真正 Flash 內容的 MD5，不把 RAM 串流校驗當作讀回校驗。 */
            mbedtls_md5_context read_hash;
            mbedtls_md5_init(&read_hash);
            verified = mbedtls_md5_starts(&read_hash) == 0;
            uint8_t block[512];
            for (uint32_t offset = 0U; verified && offset < size_;) {
                size_t count = size_ - offset; if (count > sizeof(block)) { count = sizeof(block); }
                verified = generation_connected(generation_) &&
                           esp_partition_read(partition_, offset, block, count) == ESP_OK &&
                           mbedtls_md5_update(&read_hash, block, count) == 0;
                offset += static_cast<uint32_t>(count);
            }
            if (verified) { verified = mbedtls_md5_finish(&read_hash, actual) == 0 && memcmp(actual, expected_, sizeof(actual)) == 0; }
            mbedtls_md5_free(&read_hash);
        }
        /* [修正] 在提交邊界前斷線/重連即拒絕；取得 ticket 後才開始切換開機分區。 */
        if (verified) { verified = claim_commit(generation_) && esp_ota_set_boot_partition(partition_) == ESP_OK; }
        g_verifying = false;
        return verified;
    }
    void abort() override {
        if (opened_) { (void)esp_ota_abort(handle_); opened_ = false; }
        page_used_ = 0U;
        if (hash_open_) { mbedtls_md5_free(&hash_); hash_open_ = false; }
        if (paused_) { brd_measurement_resume_after_ota(); paused_ = false; }
        g_verifying = false;
    }
};
static OtaFlash g_flash;
static brd_ota::Session g_session(g_flash);
struct ota_link_t { uint32_t generation; uint16_t handle; bool connected, subscribed; };
struct ota_request_t {
    uint32_t generation;
    uint16_t handle, length, max_data;
    bool data_characteristic;
    uint8_t data[brd_ota::DATA_HEADER + brd_ota::MAX_DATA];
};
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static ota_link_t g_link = {0U, BLE_HS_CONN_HANDLE_NONE, false, false};
static StaticQueue_t g_queue_control;
static uint8_t g_queue_storage[OTA_COMMAND_QUEUE_SIZE * sizeof(ota_request_t)];
static QueueHandle_t g_queue = nullptr;
static NimBLECharacteristic *g_status = nullptr;
static uint32_t g_seen_generation = UINT32_MAX, g_reply_generation = 0U;
static uint32_t g_notify_start_ms = 0U, g_committed_ms = 0U;
static uint16_t g_max_data = 14U;
static bool g_notify_pending = false, g_committed = false;
static uint8_t g_reply[brd_ota::RESPONSE_SIZE];
static ota_link_t link_snapshot() {
    portENTER_CRITICAL(&g_mux); ota_link_t link = g_link; portEXIT_CRITICAL(&g_mux); return link;
}
static uint32_t current_connection_generation() { return link_snapshot().generation; }
static bool generation_connected(uint32_t generation) {
    ota_link_t link = link_snapshot(); return link.connected && link.generation == generation;
}
static bool claim_commit(uint32_t generation) {
    /* [新增] 此原子檢查為提交起點；Flash API 放在 critical section 外。 */
    portENTER_CRITICAL(&g_mux);
    bool allowed = g_link.connected && g_link.generation == generation;
    portEXIT_CRITICAL(&g_mux);
    return allowed;
}
static brd_ota::Policy policy(uint16_t maximum) {
    bool charging = brd_io_is_charging();
    bool power = brd_battery_measurement_allowed() &&
                 (charging || brd_battery_get_percent() >= OTA_MIN_BATTERY_PERCENT);
    bool allowed = brd_measurement_modifications_allowed() && !brd_settings_model().pending() && brd_bbp_ota_ready();
    return {allowed, power, charging, maximum};
}
class OtaRequestCallbacks : public NimBLECharacteristicCallbacks {
    bool data_;
public:
    explicit OtaRequestCallbacks(bool data) : data_(data) {}
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        ota_link_t link = link_snapshot();
        if (!link.connected || link.handle != info.getConnHandle()) { return; }
        const auto value = characteristic->getValue();
        if (value.size() == 0U || value.size() > sizeof(ota_request_t::data)) { return; }
        ota_request_t request = {};
        request.generation = link.generation; request.handle = link.handle;
        request.length = static_cast<uint16_t>(value.size()); request.data_characteristic = data_;
        uint16_t mtu = info.getMTU();
        request.max_data = mtu > 9U ? static_cast<uint16_t>(mtu - 9U) : 0U;
        if (request.max_data > brd_ota::MAX_DATA) { request.max_data = brd_ota::MAX_DATA; }
        memcpy(request.data, value.data(), value.size());
        /* [新增] 滿列不覆寫；主機逾時讀取快照並重傳。 */
        (void)xQueueSend(g_queue, &request, 0);
    }
};
class OtaStatusCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &info, uint16_t value) override {
        portENTER_CRITICAL(&g_mux);
        if (g_link.connected && g_link.handle == info.getConnHandle()) { g_link.subscribed = (value & 1U) != 0U; }
        portEXIT_CRITICAL(&g_mux);
    }
};
static OtaRequestCallbacks g_control_callbacks(false), g_data_callbacks(true);
static OtaStatusCallbacks g_status_callbacks;
static void publish(const brd_ota::Response &response, const ota_link_t &link) {
    brd_ota::encode(response, g_reply); g_status->setValue(g_reply, sizeof(g_reply));
    g_reply_generation = link.generation; g_notify_pending = link.connected && link.subscribed;
    g_notify_start_ms = millis();
}
bool brd_ota_create(NimBLEServer *server) {
    if (server == nullptr) { return false; }
    if (g_queue == nullptr) {
        g_queue = xQueueCreateStatic(OTA_COMMAND_QUEUE_SIZE, sizeof(ota_request_t), g_queue_storage, &g_queue_control);
    }
    if (g_queue == nullptr) { return false; }
    xQueueReset(g_queue);
    NimBLEService *service = server->createService(OTA_SERVICE_UUID);
    if (service == nullptr) { return false; }
    NimBLECharacteristic *control = service->createCharacteristic(OTA_CONTROL_UUID, NIMBLE_PROPERTY::WRITE, 20U);
    NimBLECharacteristic *data = service->createCharacteristic(OTA_DATA_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR, brd_ota::DATA_HEADER + brd_ota::MAX_DATA);
    g_status = service->createCharacteristic(OTA_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 20U);
    if (control == nullptr || data == nullptr || g_status == nullptr) { return false; }
    control->setCallbacks(&g_control_callbacks); data->setCallbacks(&g_data_callbacks); g_status->setCallbacks(&g_status_callbacks);
    g_notify_pending = false; g_seen_generation = UINT32_MAX; g_max_data = 14U;
    brd_ota::encode(g_session.snapshot(brd_ota::INFO, 0, brd_ota::OK, policy(g_max_data)), g_reply);
    g_status->setValue(g_reply, sizeof(g_reply)); return true;
}
void brd_ota_connected(uint16_t handle) {
    portENTER_CRITICAL(&g_mux);
    g_link.generation++; g_link.handle = handle; g_link.connected = true; g_link.subscribed = false;
    portEXIT_CRITICAL(&g_mux);
}
void brd_ota_disconnected(uint16_t handle) {
    portENTER_CRITICAL(&g_mux);
    if (g_link.handle == handle) {
        g_link.generation++; g_link.handle = BLE_HS_CONN_HANDLE_NONE;
        g_link.connected = g_link.subscribed = false;
    }
    portEXIT_CRITICAL(&g_mux);
}
void brd_ota_stop() {
    brd_ota_disconnected(link_snapshot().handle); g_session.disconnect();
    if (g_queue != nullptr) { xQueueReset(g_queue); }
    g_status = nullptr; g_notify_pending = false;
    if (g_committed) { esp_restart(); }
}
void brd_ota_update() {
    uint32_t now = millis();
    if (g_committed && static_cast<uint32_t>(now - g_committed_ms) >= OTA_RESTART_DELAY_MS) { esp_restart(); }
    if (g_status == nullptr) { return; }
    ota_link_t link = link_snapshot();
    if (link.generation != g_seen_generation) {
        g_session.disconnect(); g_seen_generation = link.generation; g_notify_pending = false;
    }
    if (g_notify_pending && (g_reply_generation != link.generation || !link.connected || !link.subscribed ||
        static_cast<uint32_t>(now - g_notify_start_ms) >= 250U)) { g_notify_pending = false; }
    if (g_session.tick(now)) { publish(g_session.snapshot(brd_ota::INFO, 0, brd_ota::TIMEOUT, policy(g_max_data)), link); }
    if (!g_notify_pending) {
        ota_request_t request;
        if (xQueueReceive(g_queue, &request, 0) == pdTRUE) {
            /* [新增] dequeue 後再讀目前世代，禁止斷線前的舊 BEGIN 寫入 Flash。 */
            link = link_snapshot();
            if (link.connected && request.generation == link.generation && request.handle == link.handle) {
                if (g_seen_generation != link.generation) { g_session.disconnect(); g_seen_generation = link.generation; }
                g_max_data = request.max_data;
                brd_ota::Response response;
                if (request.data_characteristic != (request.data[0] == brd_ota::DATA)) {
                    response = g_session.snapshot(request.data[0], request.length > 1U ? request.data[1] : 0U,
                                                  brd_ota::BAD_COMMAND, policy(g_max_data));
                } else { response = g_session.handle(request.data, request.length, now, policy(g_max_data)); }
                ota_link_t current = link_snapshot();
                if (current.generation != link.generation || !current.connected) { g_session.disconnect(); }
                else {
                    /* [V1.2 新增] 批次 DATA 不逐包 Notify；SYNC 回覆累計 offset。
                       舊網頁不發 SYNC，仍維持原本逐包 ACK。 */
                    if (!request.data_characteristic || !g_session.batch_mode()) {
                        publish(response, link);
                    }
                }
                if (!g_committed && g_session.phase() == brd_ota::COMPLETE) { g_committed = true; g_committed_ms = millis(); }
            }
        }
    }
    link = link_snapshot();
    if (g_notify_pending && link.connected && link.subscribed && link.generation == g_reply_generation &&
        brd_scheduler_take_notify_slot() && g_status->notify(g_reply, sizeof(g_reply), link.handle)) { g_notify_pending = false; }
}
bool brd_ota_is_busy() { return g_session.busy(); }
brd_ota_display_t brd_ota_get_display() {
    auto state = g_session.snapshot(0U, 0U, brd_ota::OK, policy(g_max_data));
    uint32_t percent = state.total != 0U ? static_cast<uint32_t>((static_cast<uint64_t>(state.offset) * 100U) / state.total) : 0U;
    if (state.phase != brd_ota::COMPLETE && percent > 99U) { percent = 99U; }
    return {g_session.busy(), g_verifying, state.phase, static_cast<uint8_t>(percent)};
}
