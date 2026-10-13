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
static bool g_notify_pending = false;
static uint8_t g_packet[brd_settings::RESPONSE_SIZE] = {};
static uint32_t g_packet_generation = 0U, g_save_generation = UINT32_MAX;
static uint32_t g_last_notify_ms = 0U;
static uint32_t g_notify_started_ms = 0U;

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
    if (request == nullptr || g_response == nullptr) { return false; }
    request->setCallbacks(&g_request_callbacks);
    g_response->setCallbacks(&g_response_callbacks);
    brd_settings::make_response(brd_settings_model(), brd_settings::CMD_GET, 0U, 0U,
                               brd_settings::OK, g_packet);
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
    g_rejection_pending = g_discard_pending = false;
    portEXIT_CRITICAL(&g_mux);
    if (g_queue != nullptr) { xQueueReset(g_queue); }
    g_response = nullptr;
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
    if (g_response == nullptr) { return; }
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
            if (publish_response) { publish(link); }
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
