/*
    檔案位置: BRD_BPX/brd_settings_ble.cpp
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [新增] 獨立 BRD 設定服務；8-byte WRITE / 20-byte READ+NOTIFY。
    [新增] 量測中 GET 保留，修改回覆 BUSY 並丟棄，不延後執行。
    [新增] callback 只入列或記錄拒絕狀態；Flash 由主 loop 處理。
*/
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_settings_ble.h"
/* [V1.4.4 新增] BLE 名稱設定使用獨立 NVS；callback 只操作 RAM。 */
#include "brd_ble_identity.h"

struct settings_request_t {
    uint32_t generation;
    uint16_t handle;
    uint8_t length;
    uint8_t data[brd_settings::RESPONSE_SIZE];
};
struct settings_link_t {
    uint32_t generation;
    uint16_t handle;
    bool connected, subscribed;
};
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static settings_link_t g_link = {0U, BLE_HS_CONN_HANDLE_NONE, false, false};
static bool g_discard_pending = false;
static bool g_rejection_pending = false;
static settings_request_t g_rejection = {};
static brd_settings::Status g_rejection_status = brd_settings::BUSY;
static StaticQueue_t g_queue_control;
static uint8_t g_queue_storage[SETTINGS_COMMAND_QUEUE_SIZE * sizeof(settings_request_t)];
static QueueHandle_t g_queue = nullptr;
static NimBLECharacteristic *g_response = nullptr;
/* [新增] 1-byte Display Page GATT (READ/WRITE)。與原設定 GET/SET/SAVE 分離。 */
static NimBLECharacteristic *g_display_page_characteristic = nullptr;
static uint8_t g_display_page = BRD_DISPLAY_PAGE_MAIN;
static bool g_notify_pending = false;
static uint8_t g_packet[brd_settings::RESPONSE_SIZE] = {};
static uint32_t g_packet_generation = 0U, g_save_generation = UINT32_MAX;
static uint32_t g_last_notify_ms = 0U;
static uint32_t g_notify_started_ms = 0U;
/* [V1.4.4 新增] 主 loop 更新 4-byte 快照；BLE callback 不直接讀設定模型避免跨 task 資料競爭。 */
static uint8_t g_capacity_snapshot[4] = {};
static uint8_t g_hold_snapshot[4] = {};
static void update_read_snapshots(void) {
    const brd_settings::Manager &model = brd_settings_model();
    const uint16_t ca = model.active().battery_capacity_mah;
    const uint16_t cs = model.staged().battery_capacity_mah;
    const uint16_t ha = model.active().hold_ms;
    const uint16_t hs = model.staged().hold_ms;
    const uint8_t capacity[4] = {static_cast<uint8_t>(ca), static_cast<uint8_t>(ca >> 8U),
                                 static_cast<uint8_t>(cs), static_cast<uint8_t>(cs >> 8U)};
    const uint8_t hold[4] = {static_cast<uint8_t>(ha), static_cast<uint8_t>(ha >> 8U),
                             static_cast<uint8_t>(hs), static_cast<uint8_t>(hs >> 8U)};
    portENTER_CRITICAL(&g_mux);
    memcpy(g_capacity_snapshot, capacity, sizeof(capacity));
    memcpy(g_hold_snapshot, hold, sizeof(hold));
    portEXIT_CRITICAL(&g_mux);
}

static settings_link_t link_snapshot(void) {
    portENTER_CRITICAL(&g_mux);
    settings_link_t link = g_link;
    portEXIT_CRITICAL(&g_mux);
    return link;
}
static void reject_request(const settings_request_t &request, brd_settings::Status status) {
    portENTER_CRITICAL(&g_mux);
    g_rejection = request;
    g_rejection_status = status;
    g_rejection_pending = true;
    portEXIT_CRITICAL(&g_mux);
}
class SettingsRequestCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        settings_link_t link = link_snapshot();
        if (!link.connected || link.handle != info.getConnHandle()) { return; }
        const auto value = characteristic->getValue();
        settings_request_t request = {};
        request.generation = link.generation;
        request.handle = link.handle;
        request.length = static_cast<uint8_t>(value.size());
        size_t copy_length = value.size() < sizeof(request.data) ? value.size() : sizeof(request.data);
        for (size_t index = 0U; index < copy_length; index++) { request.data[index] = value[index]; }
        if (value.size() != brd_settings::REQUEST_SIZE) {
            reject_request(request, brd_settings::BAD_LENGTH);
            return;
        }
        if (request.data[2] != brd_settings::CMD_GET && !brd_measurement_modifications_allowed()) {
            reject_request(request, brd_settings::BUSY);
            return;
        }
        if (xQueueSend(g_queue, &request, 0) != pdTRUE) {
            /* [新增] 設定佇列滿僅回覆 BUSY，不斷線，不覆寫已接受命令。 */
            reject_request(request, brd_settings::BUSY);
        }
    }
};
class SettingsResponseCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &info, uint16_t value) override {
        portENTER_CRITICAL(&g_mux);
        if (g_link.connected && g_link.handle == info.getConnHandle()) {
            g_link.subscribed = (value & 1U) != 0U;
        }
        portEXIT_CRITICAL(&g_mux);
    }
};
/*
    [新增] 切換 OLED 顯示頁，不操作 Flash、ADC、GPIO 或量測中斷。
    READ 回傳當前有效頁面；錯誤長度/值均不改變頁面。
*/
class DisplayPageCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        auto value = characteristic->getValue();
        if (value.size() != 1U || value[0] > BRD_DISPLAY_PAGE_IR_TEST) {
            return;
        }
        portENTER_CRITICAL(&g_mux);
        if (g_link.connected && g_link.handle == info.getConnHandle()) {
            g_display_page = value[0];
        }
        portEXIT_CRITICAL(&g_mux);
    }

    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        const uint8_t current_page = brd_settings_ble_display_page();
        characteristic->setValue(&current_page, sizeof(current_page));
    }
};
static DisplayPageCallbacks g_display_page_callbacks;
/* [V1.4.4 新增] READ 韌體版本（UTF-8 ASCII，無結尾 NUL）。 */
class FirmwareVersionCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        constexpr size_t len = sizeof(PROJECT_FIRMWARE_VERSION) - 1U;
        characteristic->setValue(reinterpret_cast<const uint8_t *>(PROJECT_FIRMWARE_VERSION), len);
    }
};
/* [V1.4.4 新增] 透過 READ 查詢實際執行/草稿容量，解決 250mAh 需完整數值的問題。 */
class CapacityCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        uint8_t packet[4];
        portENTER_CRITICAL(&g_mux);
        memcpy(packet, g_capacity_snapshot, sizeof(packet));
        portEXIT_CRITICAL(&g_mux);
        characteristic->setValue(packet, sizeof(packet));
    }
};
/* [V1.4.4 新增] 透過 READ 查詢實際執行/草稿 HOLD (uint16 LE, ms)。 */
class HoldDurationCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        uint8_t packet[4];
        portENTER_CRITICAL(&g_mux);
        memcpy(packet, g_hold_snapshot, sizeof(packet));
        portEXIT_CRITICAL(&g_mux);
        characteristic->setValue(packet, sizeof(packet));
    }
};
/* [V1.4.4 新增] WRITE 名稱提交 RAM 待寫入；READ 只回已保存生效名稱。 */
class DeviceNameCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        settings_link_t link = link_snapshot();
        if (!link.connected || link.handle != info.getConnHandle()) { return; }
        const auto name = characteristic->getValue();
        (void)brd_ble_identity_request_name(reinterpret_cast<const uint8_t *>(name.data()), name.size());
    }
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        char name[BBP_DEVICE_NAME_MAX_LENGTH + 1U] = {};
        brd_ble_identity_read_name(name, sizeof(name));
        characteristic->setValue(reinterpret_cast<const uint8_t *>(name), strlen(name));
    }
};
/* [V1.4.4 新增] READ 1-byte 更名狀態；失敗/忙碌時 App 可正確辨識。 */
class DeviceNameStatusCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override {
        uint8_t status = static_cast<uint8_t>(brd_ble_identity_status());
        characteristic->setValue(&status, 1U);
    }
};
static FirmwareVersionCallbacks g_fw_version_callbacks;
static CapacityCallbacks g_capacity_callbacks;
static HoldDurationCallbacks g_hold_callbacks;
static DeviceNameCallbacks g_name_callbacks;
static DeviceNameStatusCallbacks g_name_status_callbacks;


uint8_t brd_settings_ble_display_page(void) {
    portENTER_CRITICAL(&g_mux);
    uint8_t page = g_display_page;
    portEXIT_CRITICAL(&g_mux);
    return page;
}

static SettingsRequestCallbacks g_request_callbacks;
static SettingsResponseCallbacks g_response_callbacks;

bool brd_settings_ble_create(NimBLEServer *server) {
    if (server == nullptr) { return false; }
    if (g_queue == nullptr) {
        g_queue = xQueueCreateStatic(SETTINGS_COMMAND_QUEUE_SIZE, sizeof(settings_request_t),
                                    g_queue_storage, &g_queue_control);
    }
    if (g_queue == nullptr) { return false; }
    xQueueReset(g_queue);
    NimBLEService *service = server->createService(SETTINGS_SERVICE_UUID);
    if (service == nullptr) { return false; }
    NimBLECharacteristic *request = service->createCharacteristic(
        SETTINGS_REQUEST_UUID, NIMBLE_PROPERTY::WRITE, brd_settings::RESPONSE_SIZE);
    g_response = service->createCharacteristic(SETTINGS_RESPONSE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, brd_settings::RESPONSE_SIZE);
    /* [新增] 單 byte READ/WRITE，APP 寫入後可立即 READ 確認。 */
    g_display_page_characteristic = service->createCharacteristic(
        SETTINGS_DISPLAY_PAGE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE, 1U);
    /* [V1.4.4 新增] 新特徵皆在原設定服務；既有 BBP/OTA UUID 不變。 */
    NimBLECharacteristic *firmware = service->createCharacteristic(
        SETTINGS_FW_VERSION_UUID, NIMBLE_PROPERTY::READ, sizeof(PROJECT_FIRMWARE_VERSION));
    NimBLECharacteristic *name = service->createCharacteristic(
        SETTINGS_DEVICE_NAME_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE,
        BBP_DEVICE_NAME_MAX_LENGTH);
    NimBLECharacteristic *name_status = service->createCharacteristic(
        SETTINGS_DEVICE_NAME_STATUS_UUID, NIMBLE_PROPERTY::READ, 1U);
    NimBLECharacteristic *capacity = service->createCharacteristic(
        SETTINGS_BATTERY_CAPACITY_UUID, NIMBLE_PROPERTY::READ, 4U);
    NimBLECharacteristic *hold = service->createCharacteristic(
        SETTINGS_HOLD_DURATION_UUID, NIMBLE_PROPERTY::READ, 4U);
    if (request == nullptr || g_response == nullptr || g_display_page_characteristic == nullptr ||
        firmware == nullptr || name == nullptr || name_status == nullptr ||
        capacity == nullptr || hold == nullptr) { return false; }
    firmware->setCallbacks(&g_fw_version_callbacks);
    name->setCallbacks(&g_name_callbacks);
    name_status->setCallbacks(&g_name_status_callbacks);
    capacity->setCallbacks(&g_capacity_callbacks);
    hold->setCallbacks(&g_hold_callbacks);
    request->setCallbacks(&g_request_callbacks);
    g_display_page_characteristic->setCallbacks(&g_display_page_callbacks);
    g_display_page = BRD_DISPLAY_PAGE_MAIN;
    g_display_page_characteristic->setValue(&g_display_page, sizeof(g_display_page));
    g_response->setCallbacks(&g_response_callbacks);
    brd_settings::make_response(brd_settings_model(), brd_settings::CMD_GET, 0U, 0U,
                               brd_settings::OK, g_packet);
    update_read_snapshots();
    g_response->setValue(g_packet, sizeof(g_packet));
    g_notify_pending = false;
    g_last_notify_ms = millis() - BBP_NOTIFY_INTERVAL_MS;
    return true;
}
void brd_settings_ble_connected(uint16_t handle) {
    portENTER_CRITICAL(&g_mux);
    g_link.generation++;
    g_link.handle = handle;
    g_link.connected = true;
    g_link.subscribed = false;
    /* [新增] 每次連線恢復主頁，不跨連線保留 IR TEST 診斷頁。 */
    g_display_page = BRD_DISPLAY_PAGE_MAIN;
    g_rejection_pending = false;
    g_discard_pending = true;
    portEXIT_CRITICAL(&g_mux);
}
void brd_settings_ble_disconnected(uint16_t handle) {
    portENTER_CRITICAL(&g_mux);
    if (g_link.handle == handle) {
        g_link.generation++;
        g_link.handle = BLE_HS_CONN_HANDLE_NONE;
        g_link.connected = g_link.subscribed = false;
        g_display_page = BRD_DISPLAY_PAGE_MAIN;
        g_rejection_pending = false;
        g_discard_pending = true;
    }
    portEXIT_CRITICAL(&g_mux);
}
void brd_settings_ble_stop(void) {
    portENTER_CRITICAL(&g_mux);
    g_link.generation++;
    g_link.handle = BLE_HS_CONN_HANDLE_NONE;
    g_link.connected = g_link.subscribed = false;
    g_display_page = BRD_DISPLAY_PAGE_MAIN;
    g_rejection_pending = g_discard_pending = false;
    portEXIT_CRITICAL(&g_mux);
    if (g_queue != nullptr) { xQueueReset(g_queue); }
    g_response = nullptr;
    g_display_page_characteristic = nullptr;
    g_notify_pending = false;
    brd_settings_discard_draft();
}
static void publish(const settings_link_t &link) {
    g_response->setValue(g_packet, sizeof(g_packet));
    g_packet_generation = link.generation;
    g_notify_pending = link.connected && link.subscribed;
    g_notify_started_ms = millis();
}
void brd_settings_ble_update(void) {
    /* [新增] RPM 採樣期間停用設定回覆 Notify；封包保持佇列到採樣結束。 */
    if (brd_measurement_capture_active()) { return; }
    if (g_response == nullptr) { return; }
    /* [V1.4.4 新增] 每次主 loop 更新生效/草稿快照，不在 callback 觸碰模型。 */
    update_read_snapshots();
    settings_link_t link = link_snapshot();
    portENTER_CRITICAL(&g_mux);
    bool discard = g_discard_pending;
    g_discard_pending = false;
    portEXIT_CRITICAL(&g_mux);
    if (discard) {
        brd_settings_discard_draft();
        brd_settings::make_response(brd_settings_model(), brd_settings::CMD_GET, 0U, 0U,
                                   brd_settings::OK, g_packet);
        publish(link);
    }
    if (g_notify_pending && (link.generation != g_packet_generation || !link.connected || !link.subscribed)) {
        g_notify_pending = false;
    }
    if (!g_notify_pending) {
        if (brd_settings_take_completion(g_packet)) {
            /* [新增] 保存可跨斷線完成；新連線讀狀態，不收到舊連線的 transaction 回覆。 */
            if (link.generation != g_save_generation) {
                brd_settings::make_response(brd_settings_model(), brd_settings::CMD_GET, 0U,
                                           0U, brd_settings::OK, g_packet);
            }
            publish(link);
        } else {
            settings_request_t request = {};
            brd_settings::Status rejection_status;
            portENTER_CRITICAL(&g_mux);
            bool rejected = g_rejection_pending;
            if (rejected) {
                request = g_rejection;
                rejection_status = g_rejection_status;
                g_rejection_pending = false;
            } else { rejection_status = brd_settings::OK; }
            portEXIT_CRITICAL(&g_mux);
            bool received = rejected || xQueueReceive(g_queue, &request, 0) == pdTRUE;
            bool publish_response = false;
            /*
                [修正] 取出命令後，在同一個 critical section 驗證目前連線並接受 RAM 操作。
                callback 可在 dequeue 前後斷線；不得以本輪開始時的舊 snapshot 接受 SAVE。
                此區只處理 RAM，不寫 Flash、不呼叫 NimBLE。
            */
            portENTER_CRITICAL(&g_mux);
            if (received && g_link.connected && request.generation == g_link.generation &&
                request.handle == g_link.handle) {
                link = g_link;
                if (g_discard_pending) {
                    /* [修正] 本輪中途重連時，先處理新世代草稿隔離，再接受其命令。 */
                    brd_settings_discard_draft();
                    g_discard_pending = false;
                }
                if (rejected) {
                    brd_settings::make_response(brd_settings_model(), request.data[2], request.data[3],
                                               request.data[4], rejection_status, g_packet);
                } else {
                    brd_settings::Status status = brd_settings_process_request(request.data, request.length, g_packet);
                    if (request.data[2] == brd_settings::CMD_SAVE && status == brd_settings::PENDING) {
                        g_save_generation = link.generation;
                    }
                }
                publish_response = true;
            }
            portEXIT_CRITICAL(&g_mux);
            if (publish_response) {
                update_read_snapshots();
                publish(link);
            }
        }
    }
    uint32_t now_ms = millis();
    /* [修正] 發送前重新核對連線；舊世代回覆不可送往重連後重用的 handle。 */
    link = link_snapshot();
    if (g_notify_pending && (link.generation != g_packet_generation || !link.connected || !link.subscribed)) {
        g_notify_pending = false;
    }
    if (g_notify_pending && link.connected && link.subscribed &&
        static_cast<uint32_t>(now_ms - g_last_notify_ms) >= BBP_NOTIFY_INTERVAL_MS) {
        g_last_notify_ms = now_ms;
        if (g_response->notify(g_packet, sizeof(g_packet), link.handle)) {
            g_notify_pending = false;
        } else if (static_cast<uint32_t>(now_ms - g_notify_started_ms) >= BBP_NOTIFY_TIMEOUT_MS) {
            /* [修正] 超時保留 READ 快照，但不永久阻塞後續 GET / SAVE 完成狀態。 */
            g_notify_pending = false;
        }
    }
}
