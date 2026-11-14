/*
 * 檔案：IR_Test_OTA/ir_test_ota.cpp
 * [新增] 以 BRD_BPX V1.2 brd_ota.cpp 為基礎移植到獨立 IR_Test 韌體。
 * [保留] INFO/HASH/BEGIN/DATA/SYNC/END/ABORT、Offset ACK、MD5、雙分區及 15 秒逾時。
 * [修改] 移除 BBP、RPM、NVS、充電偵測依賴；以電池 mV 安全門檻判斷。
 * [新增] 使用 IR_TEST 專屬映像標記，阻止誤燒一般 BPX 韌體。
 * [新增] BLE 回呼只入列；Flash 更新全在 loop() 執行。
 */
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mbedtls/md5.h>
#include <stdio.h>
#include <string.h>

#include "ir_test_ota.h"
#include "ir_test_ota_config.h"
#include "ir_test_ota_guard.h"

static bool g_verifying = false;
/* [OTA 新增] 電池快照由主 loop 每 50/1000ms 更新，供 Flash 中止與提交檢查。 */
static uint32_t g_battery_mv = 0U;
static uint32_t currentGeneration();
static bool generationConnected(uint32_t generation);
static bool claimCommit(uint32_t generation);

class IrTestOtaFlash : public brd_ota::Sink {
public:
    IrTestOtaFlash()
    {
        mbedtls_md5_init(&hash_);
    }

    uint32_t capacity() const override
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);

        /* [保留] 禁止更新目前正在執行的 Flash APP 分區。 */
        if (running == nullptr || next == nullptr || running->address == next->address ||
            next->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
            next->subtype > ESP_PARTITION_SUBTYPE_APP_OTA_15) {
            return 0U;
        }
        return next->size;
    }

    bool begin(uint32_t size, const uint8_t md5[16]) override
    {
        /* [修改] IR_Test 沒有 RPM 量測狀態、NVS 歷史或充電偵測，無需移植 BBP 依賴。 */
        if (size > capacity()) {
            return false;
        }
        generation_ = currentGeneration();
        if (!generationConnected(generation_)) {
            return false;
        }

        partition_ = esp_ota_get_next_update_partition(nullptr);
        if (partition_ == nullptr || esp_ota_begin(partition_, size, &handle_) != ESP_OK) {
            return false;
        }
        opened_ = true;
        page_used_ = 0U;
        size_ = size;
        memcpy(expected_, md5, sizeof(expected_));
        image_guard_.reset();
        hash_open_ = true;
        return (mbedtls_md5_starts(&hash_) == 0);
    }

    bool write(const uint8_t *data, size_t length) override
    {
        /* [OTA 新增] 傳輸中若電池跌破保護電壓或 ADC 異常，立即失敗保留舊 APP。 */
        if (!opened_ || data == nullptr ||
            g_battery_mv < IR_OTA_MIN_BATTERY_MV ||
            g_battery_mv > IR_OTA_MAX_BATTERY_MV ||
            mbedtls_md5_update(&hash_, data, length) != 0) {
            return false;
        }

        /* [新增] 串流偵測 IR_Test 專屬韌體標記，避免燒入普通 BRD_BPX 映像。 */
        image_guard_.append(data, length);
        while (length != 0U) {
            size_t count = sizeof(page_) - page_used_;
            if (count > length) {
                count = length;
            }
            memcpy(page_ + page_used_, data, count);
            page_used_ += count;
            data += count;
            length -= count;
            if (page_used_ == sizeof(page_) && !flushPage()) {
                return false;
            }
        }
        return true;
    }

    bool finish() override
    {
        g_verifying = true;
        /* [新增] 在阻塞式 Flash 讀回校驗前先立即刷新 OTA VERIFY 畫面。 */
        irTestOtaRenderNow();

        uint8_t actual[16] = {};
        /* [保留] 最後不足 4 KiB 的資料仍須寫完並檢查 MD5。 */
        bool verified = image_guard_.valid() && opened_ &&
                        g_battery_mv >= IR_OTA_MIN_BATTERY_MV &&
                        g_battery_mv <= IR_OTA_MAX_BATTERY_MV && flushPage() &&
                        mbedtls_md5_finish(&hash_, actual) == 0 &&
                        memcmp(actual, expected_, sizeof(actual)) == 0;

        if (verified) {
            /* [保留] esp_ota_end 驗證 ESP32 映像格式。 */
            esp_err_t result = esp_ota_end(handle_);
            opened_ = false;
            verified = (result == ESP_OK);
        }
        if (hash_open_) {
            mbedtls_md5_free(&hash_);
            hash_open_ = false;
        }

        if (verified) {
            /* [保留] 對 Flash 實際讀回內容重新計算 MD5，非只驗證傳輸 RAM。 */
            mbedtls_md5_context read_hash;
            mbedtls_md5_init(&read_hash);
            verified = (mbedtls_md5_starts(&read_hash) == 0);
            uint8_t block[512];
            for (uint32_t offset = 0U; verified && offset < size_;) {
                size_t count = size_ - offset;
                if (count > sizeof(block)) {
                    count = sizeof(block);
                }
                verified = generationConnected(generation_) &&
                           esp_partition_read(partition_, offset, block, count) == ESP_OK &&
                           mbedtls_md5_update(&read_hash, block, count) == 0;
                offset += static_cast<uint32_t>(count);
            }
            if (verified) {
                verified = (mbedtls_md5_finish(&read_hash, actual) == 0) &&
                           (memcmp(actual, expected_, sizeof(actual)) == 0);
            }
            mbedtls_md5_free(&read_hash);
        }

        /* [保留] 提交前最後一次核對 BLE 連線世代。 */
        if (verified) {
            verified = claimCommit(generation_) &&
                       g_battery_mv >= IR_OTA_MIN_BATTERY_MV &&
                       g_battery_mv <= IR_OTA_MAX_BATTERY_MV &&
                       esp_ota_set_boot_partition(partition_) == ESP_OK;
        }
        g_verifying = false;
        return verified;
    }

    void abort() override
    {
        if (opened_) {
            (void)esp_ota_abort(handle_);
            opened_ = false;
        }
        page_used_ = 0U;
        if (hash_open_) {
            mbedtls_md5_free(&hash_);
            hash_open_ = false;
        }
        image_guard_.reset();
        g_verifying = false;
    }

private:
    bool flushPage()
    {
        if (page_used_ == 0U) {
            return true;
        }
        if (!opened_ || esp_ota_write(handle_, page_, page_used_) != ESP_OK) {
            return false;
        }
        page_used_ = 0U;
        return true;
    }

    const esp_partition_t *partition_ = nullptr;
    esp_ota_handle_t handle_ = 0U;
    uint32_t size_ = 0U;
    uint32_t generation_ = UINT32_MAX;
    bool opened_ = false;
    bool hash_open_ = false;
    uint8_t expected_[16] = {};
    uint8_t page_[4096] = {};
    size_t page_used_ = 0U;
    mbedtls_md5_context hash_;
    IrTestOtaGuard image_guard_;
};

/* [沿用] 封包工作佇列只在主 loop 讀取。 */
static IrTestOtaFlash g_flash;
static brd_ota::Session g_session(g_flash);

struct OtaLink {
    uint32_t generation;
    uint16_t handle;
    bool connected;
    bool subscribed;
};

struct OtaRequest {
    uint32_t generation;
    uint16_t handle;
    uint16_t length;
    uint16_t max_data;
    bool is_data;
    uint8_t data[brd_ota::DATA_HEADER + brd_ota::MAX_DATA];
};

static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static OtaLink g_link = {0U, BLE_HS_CONN_HANDLE_NONE, false, false};
static StaticQueue_t g_queue_control;
static uint8_t g_queue_storage[IR_OTA_QUEUE_LENGTH * sizeof(OtaRequest)];
static QueueHandle_t g_queue = nullptr;
static NimBLECharacteristic *g_status = nullptr;
static NimBLEAdvertising *g_advertising = nullptr;
static uint32_t g_seen_generation = UINT32_MAX;
static uint32_t g_reply_generation = 0U;
static uint32_t g_notify_start_ms = 0U;
static uint32_t g_committed_ms = 0U;
static uint32_t g_last_advertise_retry_ms = 0U;
static uint32_t g_error_ms = 0U;
static uint16_t g_max_data = 14U;
static bool g_notify_pending = false;
static bool g_committed = false;
static bool g_started = false;
static uint8_t g_reply[brd_ota::RESPONSE_SIZE] = {};
static char g_name[sizeof(IR_OTA_DEVICE_PREFIX) + 4U] = {};

static OtaLink linkSnapshot()
{
    portENTER_CRITICAL(&g_mux);
    OtaLink value = g_link;
    portEXIT_CRITICAL(&g_mux);
    return value;
}

static uint32_t currentGeneration()
{
    return linkSnapshot().generation;
}

static bool generationConnected(uint32_t generation)
{
    OtaLink link = linkSnapshot();
    return link.connected && link.generation == generation;
}

static bool claimCommit(uint32_t generation)
{
    portENTER_CRITICAL(&g_mux);
    bool allowed = g_link.connected && g_link.generation == generation;
    portEXIT_CRITICAL(&g_mux);
    return allowed;
}

static brd_ota::Policy policy(uint16_t maxData)
{
    /* [修改] IR_Test 電池以 mV 判斷，沒有 CHRG_DET；不臆測充電中可 OTA。 */
    bool power = g_battery_mv >= IR_OTA_MIN_BATTERY_MV &&
                 g_battery_mv <= IR_OTA_MAX_BATTERY_MV;
    return {power && !g_committed, power, false, maxData};
}

class IrTestServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override
    {
        bool reject = false;
        portENTER_CRITICAL(&g_mux);
        if (g_link.connected || g_committed) {
            reject = true;
        } else {
            g_link.generation++;
            g_link.handle = info.getConnHandle();
            g_link.connected = true;
            g_link.subscribed = false;
        }
        portEXIT_CRITICAL(&g_mux);
        if (reject) {
            server->disconnect(info.getConnHandle());
        }
    }

    void onDisconnect(NimBLEServer *, NimBLEConnInfo &info, int reason) override
    {
        (void)reason;
        portENTER_CRITICAL(&g_mux);
        if (g_link.handle == info.getConnHandle()) {
            g_link.generation++;
            g_link.handle = BLE_HS_CONN_HANDLE_NONE;
            g_link.connected = false;
            g_link.subscribed = false;
        }
        portEXIT_CRITICAL(&g_mux);
        /* [新增] 廣播在主 loop 恢復，不從 GAP callback 同步啟動。 */
    }
};

class IrTestRequestCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit IrTestRequestCallbacks(bool isData) : is_data_(isData) {}

    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override
    {
        OtaLink link = linkSnapshot();
        if (!link.connected || link.handle != info.getConnHandle() || g_queue == nullptr) {
            return;
        }
        const auto value = characteristic->getValue();
        /* [V1.2.1 修正] NimBLEAttValue 不提供 empty()；改用 size() 判斷空封包。 */
        if (value.size() == 0U || value.size() > sizeof(OtaRequest::data)) {
            return;
        }
        OtaRequest request = {};
        request.generation = link.generation;
        request.handle = link.handle;
        request.length = static_cast<uint16_t>(value.size());
        request.is_data = is_data_;
        uint16_t mtu = info.getMTU();
        request.max_data = mtu > 9U ? static_cast<uint16_t>(mtu - 9U) : 0U;
        if (request.max_data > brd_ota::MAX_DATA) {
            request.max_data = brd_ota::MAX_DATA;
        }
        memcpy(request.data, value.data(), value.size());
        /* [保留] 滿列不覆寫；主機可以透過 STATUS/SYNC 重建 Offset。 */
        (void)xQueueSend(g_queue, &request, 0);
    }

private:
    bool is_data_;
};

class IrTestStatusCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &info, uint16_t value) override
    {
        portENTER_CRITICAL(&g_mux);
        if (g_link.connected && g_link.handle == info.getConnHandle()) {
            g_link.subscribed = (value & 1U) != 0U;
        }
        portEXIT_CRITICAL(&g_mux);
    }
};

static IrTestServerCallbacks g_server_callbacks;
static IrTestRequestCallbacks g_control_callbacks(false);
static IrTestRequestCallbacks g_data_callbacks(true);
static IrTestStatusCallbacks g_status_callbacks;

static void publish(const brd_ota::Response &response, const OtaLink &link)
{
    brd_ota::encode(response, g_reply);
    g_status->setValue(g_reply, sizeof(g_reply));
    g_reply_generation = link.generation;
    g_notify_pending = link.connected && link.subscribed;
    g_notify_start_ms = millis();
    if (response.phase == brd_ota::ERROR) {
        g_error_ms = g_notify_start_ms;
    }
}

void irTestOtaSetBatteryMv(uint32_t batteryMv)
{
    g_battery_mv = batteryMv;
}

bool irTestOtaBegin()
{
    if (g_started) {
        return true;
    }
    if (g_flash.capacity() == 0U) {
        /* [新增] 若裝置不是雙 OTA 分區，不開放無線升級。 */
        return false;
    }
    if (g_queue == nullptr) {
        g_queue = xQueueCreateStatic(IR_OTA_QUEUE_LENGTH, sizeof(OtaRequest),
                                     g_queue_storage, &g_queue_control);
    }
    if (g_queue == nullptr) {
        return false;
    }
    xQueueReset(g_queue);

    uint8_t uid[6] = {};
    if (esp_efuse_mac_get_default(uid) != ESP_OK) {
        return false;
    }
    snprintf(g_name, sizeof(g_name), "%s%02X%02X", IR_OTA_DEVICE_PREFIX, uid[4], uid[5]);
    if (!NimBLEDevice::init(g_name)) {
        return false;
    }
    bool success = NimBLEDevice::setMTU(IR_OTA_MTU);
    success = NimBLEDevice::setPower(0) && success;

    NimBLEServer *server = NimBLEDevice::createServer();
    if (server == nullptr) {
        return false;
    }
    server->setCallbacks(&g_server_callbacks, false);
    server->advertiseOnDisconnect(false);
    /*
     * [OTA Web 相容新增] 原 Web 以 BBP discovery UUID 篩選裝置。
     * 這裡只建立只讀身分特徵值作探索，不實作 BBP 命令/量測功能。
     */
    NimBLEService *discovery = server->createService(IR_OTA_DISCOVERY_SERVICE_UUID);
    if (discovery == nullptr) {
        return false;
    }
    NimBLECharacteristic *identity = discovery->createCharacteristic(
        IR_OTA_DISCOVERY_IDENTITY_UUID, NIMBLE_PROPERTY::READ, 16U);
    if (identity == nullptr) {
        return false;
    }
    const char identity_text[] = "IR_TEST_OTA_V1";
    identity->setValue(reinterpret_cast<const uint8_t *>(identity_text),
                       sizeof(identity_text) - 1U);

    NimBLEService *service = server->createService(IR_OTA_SERVICE_UUID);
    if (service == nullptr) {
        return false;
    }
    NimBLECharacteristic *control = service->createCharacteristic(
        IR_OTA_CONTROL_UUID, NIMBLE_PROPERTY::WRITE, 20U);
    NimBLECharacteristic *data = service->createCharacteristic(
        IR_OTA_DATA_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        brd_ota::DATA_HEADER + brd_ota::MAX_DATA);
    g_status = service->createCharacteristic(IR_OTA_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 20U);
    if (control == nullptr || data == nullptr || g_status == nullptr) {
        return false;
    }
    control->setCallbacks(&g_control_callbacks);
    data->setCallbacks(&g_data_callbacks);
    g_status->setCallbacks(&g_status_callbacks);
    brd_ota::encode(g_session.snapshot(brd_ota::INFO, 0, brd_ota::OK, policy(g_max_data)), g_reply);
    g_status->setValue(g_reply, sizeof(g_reply));
    success = server->start() && success;

    /*
     * [OTA Web 相容修改] 原 Web 掃描條件是 BBP discovery UUID + 名稱前綴，
     * OTA UUID 經 optionalServices 授權後在 GATT 讀取，與參考韌體一致。
     */
    NimBLEAdvertisementData advertisement;
    NimBLEAdvertisementData scan_response;
    success = advertisement.setFlags(0x06U) && success;
    success = advertisement.addServiceUUID(IR_OTA_DISCOVERY_SERVICE_UUID) && success;
    success = scan_response.setName(g_name) && success;
    g_advertising = NimBLEDevice::getAdvertising();
    if (g_advertising != nullptr) {
        g_advertising->enableScanResponse(true);
        success = g_advertising->setAdvertisementData(advertisement) && success;
        success = g_advertising->setScanResponseData(scan_response) && success;
    } else {
        success = false;
    }
    if (!success || !g_advertising->start()) {
        return false;
    }
    g_started = true;
    return true;
}

void irTestOtaUpdate()
{
    uint32_t now = millis();
    if (g_committed && static_cast<uint32_t>(now - g_committed_ms) >= IR_OTA_RESTART_DELAY_MS) {
        esp_restart();
    }
    if (!g_started || g_status == nullptr) {
        return;
    }
    OtaLink link = linkSnapshot();
    if (link.generation != g_seen_generation) {
        g_session.disconnect();
        g_seen_generation = link.generation;
        g_notify_pending = false;
    }
    if (g_notify_pending && (g_reply_generation != link.generation || !link.connected ||
        !link.subscribed || static_cast<uint32_t>(now - g_notify_start_ms) >= 250U)) {
        g_notify_pending = false;
    }
    if (g_session.tick(now)) {
        publish(g_session.snapshot(brd_ota::INFO, 0, brd_ota::TIMEOUT, policy(g_max_data)), link);
    }

    if (!g_notify_pending) {
        OtaRequest request;
        if (xQueueReceive(g_queue, &request, 0) == pdTRUE) {
            link = linkSnapshot();
            if (link.connected && request.generation == link.generation &&
                request.handle == link.handle) {
                if (g_seen_generation != link.generation) {
                    g_session.disconnect();
                    g_seen_generation = link.generation;
                }
                g_max_data = request.max_data;
                brd_ota::Response response;
                if (request.is_data != (request.data[0] == brd_ota::DATA)) {
                    response = g_session.snapshot(request.data[0], request.length > 1U ? request.data[1] : 0U,
                                                  brd_ota::BAD_COMMAND, policy(g_max_data));
                } else {
                    response = g_session.handle(request.data, request.length, now, policy(g_max_data));
                }
                OtaLink current = linkSnapshot();
                if (!current.connected || current.generation != link.generation) {
                    g_session.disconnect();
                } else {
                    /* [沿用] DATA batch 模式只用 SYNC 回傳累計位移，降低 Notify 次數。 */
                    if (!request.is_data || !g_session.batch_mode()) {
                        publish(response, link);
                    }
                }
                if (!g_committed && g_session.phase() == brd_ota::COMPLETE) {
                    g_committed = true;
                    g_committed_ms = millis();
                }
            }
        }
    }
    link = linkSnapshot();
    if (g_notify_pending && link.connected && link.subscribed &&
        link.generation == g_reply_generation &&
        g_status->notify(g_reply, sizeof(g_reply), link.handle)) {
        g_notify_pending = false;
    }

    /* [新增] 不依賴原 BBP 主程式，斷線後由本模組定期重新開始廣播。 */
    if (!g_committed && !link.connected && g_advertising != nullptr &&
        static_cast<uint32_t>(now - g_last_advertise_retry_ms) >= 1000U) {
        g_last_advertise_retry_ms = now;
        if (!g_advertising->isAdvertising()) {
            (void)g_advertising->start();
        }
    }
}

bool irTestOtaBusy()
{
    return g_session.busy();
}

IrTestOtaDisplay irTestOtaGetDisplay()
{
    brd_ota::Response state = g_session.snapshot(0U, 0U, brd_ota::OK, policy(g_max_data));
    uint32_t percent = (state.total != 0U) ?
        static_cast<uint32_t>((static_cast<uint64_t>(state.offset) * 100U) / state.total) : 0U;
    if (state.phase != brd_ota::COMPLETE && percent > 99U) {
        percent = 99U;
    }
    bool error_visible = state.phase == brd_ota::ERROR &&
        static_cast<uint32_t>(millis() - g_error_ms) < 3000U;
    return {g_session.busy() || g_verifying || error_visible,
            g_verifying, state.phase, static_cast<uint8_t>(percent)};
}
