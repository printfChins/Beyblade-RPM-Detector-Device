/*
    檔案位置: BRD_BLE_OLED/brd_ble.cpp
    [V1.15 修改] Reliable BLE Protocol V4：恢復 B1 每 200 ms 持續同步狀態，移除 0x81/C5/state_seq。
    [V1.15 保留] 韌體不因通訊逾時主動斷線。
    [V1.11 保留] START/DATA/END/A4 timeout、Session、Packet Index、CRC32、ACK/重傳/Abort。
    [保留] NimBLE callback 只更新連線快照與有界命令佇列；封包處理由主 loop 執行。
*/
#include <NimBLEDevice.h>
#include <cstdio>
#include <cstring>

#include "brd_ble.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_record.h"

struct ble_link_t {
    uint32_t session;
    uint16_t handle;
    bool connected;
    bool notify_subscribed;
};

struct ble_command_t {
    uint32_t session;
    uint8_t size;
    uint8_t bytes[20];
};

struct ble_reliable_status_t {
    bool pending;
    uint16_t session;
    uint16_t detail;
    uint8_t code;
    uint8_t after;
    uint32_t start_ms;
};

static portMUX_TYPE g_ble_mux = portMUX_INITIALIZER_UNLOCKED;
static ble_link_t g_ble_link = {};
static bool g_ble_accept_connections = false;
static ble_command_t g_ble_commands[BLE_COMMAND_QUEUE_SIZE];
static uint8_t g_ble_command_head = 0U;
static uint8_t g_ble_command_tail = 0U;
static uint32_t g_ble_callback_errors = 0UL;
static NimBLEServer *g_ble_server = nullptr;
static NimBLECharacteristic *g_ble_notify = nullptr;
static NimBLECharacteristic *g_ble_control = nullptr;
static NimBLECharacteristic *g_ble_firmware = nullptr;
static NimBLECharacteristic *g_ble_diagnostic = nullptr;
static NimBLEAdvertising *g_ble_advertising = nullptr;
static brd_ble_diagnostics_t g_ble_diag = {};
static char g_ble_name[BLE_DEVICE_NAME_MAX_LEN] = {};
static bool g_ble_init_attempted = false;
static bool g_ble_teardown_pending = false;
static uint32_t g_ble_last_shutdown_ms = 0UL;
static uint32_t g_ble_last_init_ms = 0UL;
static uint32_t g_ble_last_adv_ms = 0UL;
static uint32_t g_ble_session_seen = UINT32_MAX;
static uint32_t g_ble_epoch_seen = UINT32_MAX;
static uint32_t g_ble_subscribe_ms = 0UL;
static uint32_t g_ble_last_packet_ms = 0UL;
static uint32_t g_ble_last_live_ms = 0UL;
static uint32_t g_ble_tx_start_ms = 0UL;
static uint32_t g_ble_ack_start_ms = 0UL;
static uint32_t g_ble_last_end_ms = 0UL;
static bool g_ble_ack_clock_started = false;
static bool g_ble_launch_sent = false;
static bool g_ble_have_live = false;
static uint8_t g_ble_control_value[20] = {};
static ble_reliable_status_t g_reliable_status = {};
static uint16_t g_reliable_packet_index = 0U;
static uint16_t g_reliable_retry_indexes[8] = {};
static uint8_t g_reliable_retry_count = 0U;
static uint8_t g_reliable_retry_cursor = 0U;
static bool g_reliable_retrying = false;
static uint16_t g_reliable_last_ack_session = 0U;
static uint16_t g_reliable_last_ack_count = 0U;
static uint32_t g_reliable_last_ack_crc = 0UL;
static bool g_reliable_last_ack_valid = false;

static void ble_count(uint32_t &value) {
    if (value != UINT32_MAX) {
        value++;
    }
}

static void ble_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void ble_u32(uint8_t *out, uint32_t value) {
    for (uint8_t i = 0U; i < 4U; i++) {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

static uint32_t ble_read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint16_t ble_read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint16_t ble_protocol_session(const brd_record_info_t &record) {
    return (uint16_t)((record.result_id - 1UL) % 65534UL + 1UL);
}

static ble_link_t ble_get_link(void) {
    portENTER_CRITICAL(&g_ble_mux);
    ble_link_t link = g_ble_link;
    portEXIT_CRITICAL(&g_ble_mux);
    return link;
}

class BrdServerCallbacks final : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
        portENTER_CRITICAL(&g_ble_mux);
        bool accept = g_ble_accept_connections && !g_ble_link.connected;
        if (accept) {
            g_ble_link.session++;
            g_ble_link.handle = info.getConnHandle();
            g_ble_link.connected = true;
            g_ble_link.notify_subscribed = false;
            g_ble_command_head = g_ble_command_tail = 0U;
        }
        portEXIT_CRITICAL(&g_ble_mux);
        (void)server;
        /* [V1.15 保留] 不主動斷線；非目前追蹤連線不進入 BRD link state。 */
    }

    void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override {
        (void)server;
        (void)reason;
        portENTER_CRITICAL(&g_ble_mux);
        if (g_ble_link.connected && g_ble_link.handle == info.getConnHandle()) {
            g_ble_link.connected = false;
            g_ble_link.notify_subscribed = false;
            g_ble_link.session++;
            g_ble_command_head = g_ble_command_tail = 0U;
        }
        portEXIT_CRITICAL(&g_ble_mux);
        /* [V1.11 保留] 由主 loop 決定是否重新廣播，斷線不清除尚未 ACK 的曲線。 */
    }
};

class BrdNotifyCallbacks final : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *characteristic, NimBLEConnInfo &info,
                     uint16_t subscription) override {
        if (characteristic != g_ble_notify) {
            return;
        }
        portENTER_CRITICAL(&g_ble_mux);
        bool enabled = (subscription & 1U) != 0U;
        if (g_ble_link.connected && g_ble_link.handle == info.getConnHandle() &&
            enabled != g_ble_link.notify_subscribed) {
            g_ble_link.notify_subscribed = enabled;
            g_ble_link.session++;
            g_ble_command_head = g_ble_command_tail = 0U;
        }
        portEXIT_CRITICAL(&g_ble_mux);
    }
};

class BrdControlCallbacks final : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        (void)info;
        if (characteristic == g_ble_firmware) {
            characteristic->setValue(PROJECT_VERSION);
            return;
        }
        if (characteristic == g_ble_diagnostic) {
            uint8_t value[20];
            portENTER_CRITICAL(&g_ble_mux);
            memcpy(value, g_ble_control_value, sizeof(value));
            portEXIT_CRITICAL(&g_ble_mux);
            characteristic->setValue(value, sizeof(value));
            return;
        }
    }

    void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &info) override {
        NimBLEAttValue value = characteristic->getValue();
        portENTER_CRITICAL(&g_ble_mux);
        uint8_t next = (uint8_t)((g_ble_command_head + 1U) % BLE_COMMAND_QUEUE_SIZE);
        if (characteristic != g_ble_control || !g_ble_accept_connections ||
            !g_ble_link.connected || !g_ble_link.notify_subscribed ||
            g_ble_link.handle != info.getConnHandle() || value.size() == 0U ||
            value.size() > sizeof(g_ble_commands[0].bytes) || next == g_ble_command_tail) {
            ble_count(g_ble_callback_errors);
        } else {
            ble_command_t &command = g_ble_commands[g_ble_command_head];
            command.session = g_ble_link.session;
            command.size = (uint8_t)value.size();
            for (uint8_t i = 0U; i < command.size; i++) {
                command.bytes[i] = value.data()[i];
            }
            g_ble_command_head = next;
        }
        portEXIT_CRITICAL(&g_ble_mux);
    }
};

static BrdServerCallbacks g_ble_server_callbacks;
static BrdNotifyCallbacks g_ble_notify_callbacks;
static BrdControlCallbacks g_ble_control_callbacks;

static void ble_reset_transfer(void) {
    g_ble_diag.tx_state = BRD_BLE_TX_IDLE;
    g_ble_ack_clock_started = false;
    g_ble_launch_sent = false;
    g_reliable_packet_index = 0U;
    g_reliable_retry_count = 0U;
    g_reliable_retry_cursor = 0U;
    g_reliable_retrying = false;
}

static bool ble_shutdown(void) {
    ble_link_t previous_link = ble_get_link();
    g_ble_last_shutdown_ms = millis();
    portENTER_CRITICAL(&g_ble_mux);
    g_ble_accept_connections = false;
    g_ble_link.connected = false;
    g_ble_link.notify_subscribed = false;
    g_ble_link.session++;
    g_ble_command_head = g_ble_command_tail = 0U;
    portEXIT_CRITICAL(&g_ble_mux);
    if (g_ble_advertising != nullptr) {
        g_ble_advertising->stop();
    }
    (void)previous_link;
    /* [V1.15 保留] 不主動呼叫 BLE 斷線 API；BLE stack deinit 僅用於功能停用/睡眠關閉。 */
    g_ble_diag.initialized = false;
    g_ble_diag.connected = false;
    g_ble_diag.subscribed = false;
    g_ble_diag.reliable_mode = false;
    g_reliable_status = {};
    g_ble_have_live = false;
    ble_reset_transfer();
    if (NimBLEDevice::isInitialized() && !NimBLEDevice::deinit(false)) {
        g_ble_teardown_pending = true;
        ble_count(g_ble_diag.shutdown_failures);
        return false;
    }
    NimBLEDevice::deinit(true);
    g_ble_server = nullptr;
    g_ble_notify = nullptr;
    g_ble_control = nullptr;
    g_ble_firmware = nullptr;
    g_ble_diagnostic = nullptr;
    g_ble_advertising = nullptr;
    g_ble_teardown_pending = false;
    return true;
}

static bool ble_initialize(void) {
    /*
        [V1.15 修改] BLE 名稱固定為 BRD_XXXX。
        XXXX 直接取 ESP32-C3 eFuse MAC 最低 16-bit，以 4 碼大寫 HEX 顯示。
        例: eFuse MAC ...A1B2 -> BRD_A1B2。
    */
    uint16_t suffix = (uint16_t)(ESP.getEfuseMac() & BLE_DEVICE_SUFFIX_MASK);
    snprintf(g_ble_name, sizeof(g_ble_name), "%s_%04X", BLE_DEVICE_NAME_PREFIX, (unsigned)suffix);
    if (!NimBLEDevice::init(g_ble_name)) {
        return false;
    }
    if (!NimBLEDevice::setPower(BLE_TX_POWER_DBM) || !NimBLEDevice::setMTU(23U)) {
        return false;
    }
    g_ble_server = NimBLEDevice::createServer();
    if (g_ble_server == nullptr) {
        return false;
    }
    g_ble_server->setCallbacks(&g_ble_server_callbacks, false);
    g_ble_server->advertiseOnDisconnect(false);
    NimBLEService *service = g_ble_server->createService(BRD_SERVICE_UUID);
    if (service == nullptr) {
        return false;
    }
    g_ble_notify = service->createCharacteristic(BRD_NOTIFY_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 20U);
    g_ble_control = service->createCharacteristic(BRD_CONTROL_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE, 20U);
    g_ble_firmware = service->createCharacteristic(BRD_FIRMWARE_REVISION_CHAR_UUID,
        NIMBLE_PROPERTY::READ, 16U);
    g_ble_diagnostic = service->createCharacteristic(BRD_DIAGNOSTIC_CHAR_UUID,
        NIMBLE_PROPERTY::READ, 20U);
    if (g_ble_notify == nullptr || g_ble_control == nullptr || g_ble_firmware == nullptr ||
        g_ble_diagnostic == nullptr) {
        return false;
    }
    g_ble_notify->setCallbacks(&g_ble_notify_callbacks);
    g_ble_control->setCallbacks(&g_ble_control_callbacks);
    g_ble_firmware->setCallbacks(&g_ble_control_callbacks);
    g_ble_diagnostic->setCallbacks(&g_ble_control_callbacks);
    uint8_t live[13] = {0xB1U};
    g_ble_notify->setValue(live, sizeof(live));
    g_ble_firmware->setValue(PROJECT_VERSION);
    memset(g_ble_control_value, 0, sizeof(g_ble_control_value));
    g_ble_control_value[0] = 4U;
    if (!g_ble_server->start()) {
        return false;
    }
    g_ble_advertising = NimBLEDevice::getAdvertising();
    if (g_ble_advertising == nullptr) {
        return false;
    }
    g_ble_advertising->enableScanResponse(true);
    if (!g_ble_advertising->addServiceUUID(BRD_SERVICE_UUID) ||
        !g_ble_advertising->setName(g_ble_name)) {
        return false;
    }
    g_ble_advertising->setMinInterval(160U);
    g_ble_advertising->setMaxInterval(800U);
    portENTER_CRITICAL(&g_ble_mux);
    g_ble_accept_connections = true;
    portEXIT_CRITICAL(&g_ble_mux);
    return g_ble_advertising->start();
}

void brd_ble_set_enabled(bool enabled) {
    if (g_ble_teardown_pending) {
        if ((uint32_t)(millis() - g_ble_last_shutdown_ms) < BLE_INIT_RETRY_MS || !ble_shutdown()) {
            return;
        }
    }
    if (!enabled) {
        if (g_ble_diag.initialized || NimBLEDevice::isInitialized()) {
            (void)ble_shutdown();
        }
        g_ble_init_attempted = false;
        return;
    }
    if (g_ble_diag.initialized) {
        return;
    }
    uint32_t now_ms = millis();
    if (g_ble_init_attempted && (uint32_t)(now_ms - g_ble_last_init_ms) < BLE_INIT_RETRY_MS) {
        return;
    }
    g_ble_init_attempted = true;
    g_ble_last_init_ms = now_ms;
    if (!ble_initialize()) {
        ble_count(g_ble_diag.init_failures);
        (void)ble_shutdown();
        return;
    }
    g_ble_diag.initialized = true;
    g_ble_diag.reliable_mode = true;
    g_ble_last_adv_ms = now_ms;
}

static bool ble_pop_command(ble_command_t &command) {
    portENTER_CRITICAL(&g_ble_mux);
    bool available = g_ble_command_head != g_ble_command_tail;
    if (available) {
        command = g_ble_commands[g_ble_command_tail];
        g_ble_command_tail = (uint8_t)((g_ble_command_tail + 1U) % BLE_COMMAND_QUEUE_SIZE);
    }
    portEXIT_CRITICAL(&g_ble_mux);
    return available;
}

static void ble_start_transfer(uint32_t now_ms, bool new_window) {
    g_ble_diag.tx_state = BRD_BLE_TX_START;
    g_reliable_packet_index = 0U;
    g_reliable_retrying = false;
    g_ble_tx_start_ms = now_ms;
    if (new_window) {
        g_ble_ack_clock_started = false;
    }
}

static void ble_status(uint16_t session, uint8_t status, uint16_t detail,
                       uint32_t now_ms, uint8_t after = 0U) {
    g_reliable_status = {true, session, detail, status, after, now_ms};
}

static void ble_apply_status_after(uint8_t after, uint32_t now_ms) {
    if (after == 1U) {
        /* [V1.11 修改] C1 是交易完成依據；A4 若失敗也不可永久卡住或保留舊曲線。 */
        g_reliable_last_ack_valid = true;
        brd_record_reset();
        g_ble_diag.tx_state = BRD_BLE_TX_DONE;
        g_ble_ack_clock_started = false;
    } else if (after == 2U) {
        g_ble_ack_clock_started = false;
        g_ble_tx_start_ms = now_ms;
        g_reliable_retrying = g_reliable_retry_count != 0U;
        g_ble_diag.tx_state = g_reliable_retrying ? BRD_BLE_TX_DATA : BRD_BLE_TX_END;
    } else if (after == 3U) {
        ble_start_transfer(now_ms, true);
        ble_count(g_ble_diag.retransmissions);
    } else if (after == 4U) {
        brd_record_reset();
        g_ble_diag.tx_state = BRD_BLE_TX_DONE;
        g_ble_ack_clock_started = false;
    }
}

static void ble_command(const ble_command_t &command, const brd_record_info_t &record,
                        uint32_t now_ms) {
    const uint8_t *p = command.bytes;
    uint16_t requested = command.size >= 3U ? ble_read_u16(p + 1U) : 0U;
    uint16_t session = record.ready ? ble_protocol_session(record) : 0U;
    brd_event_info_t events = brd_record_get_event_info();

    if (command.size < 3U) {
        ble_count(g_ble_diag.command_errors);
        ble_status(requested, 0x82U, command.size, now_ms);
        return;
    }
    if (p[0] == 0xC1U && command.size == 9U && g_reliable_last_ack_valid &&
        requested == g_reliable_last_ack_session && ble_read_u16(p + 3U) == g_reliable_last_ack_count &&
        ble_read_u32(p + 5U) == g_reliable_last_ack_crc) {
        /* [V1.11 保留] Duplicate ACK 為 idempotent，只重回 A4，不碰目前新結果。 */
        ble_status(requested, 0x00U, 0U, now_ms);
        return;
    }
    if (!record.ready) {
        ble_count(g_ble_diag.command_errors);
        ble_status(requested, 0x85U, 0U, now_ms);
        return;
    }
    if (requested != session && !(p[0] == 0xC3U && requested == 0xFFFFU)) {
        ble_count(g_ble_diag.command_errors);
        ble_status(requested, 0x80U, session, now_ms);
        return;
    }

    if (p[0] == 0xC1U) {
        if (command.size != 9U) {
            ble_count(g_ble_diag.command_errors);
            ble_status(requested, 0x82U, command.size, now_ms);
        } else if (ble_read_u16(p + 3U) != events.count) {
            ble_count(g_ble_diag.command_errors);
            ble_status(requested, 0x83U, events.count, now_ms);
        } else if (ble_read_u32(p + 5U) != events.crc32) {
            ble_count(g_ble_diag.command_errors);
            ble_status(requested, 0x84U, 0U, now_ms);
        } else if (g_ble_diag.tx_state != BRD_BLE_TX_WAIT_ACK ||
                   (uint32_t)(now_ms - g_ble_ack_start_ms) >= BLE_ACK_TIMEOUT_MS) {
            ble_count(g_ble_diag.command_errors);
            ble_status(requested, 0x86U, 0U, now_ms);
        } else {
            g_reliable_last_ack_session = session;
            g_reliable_last_ack_count = events.count;
            g_reliable_last_ack_crc = events.crc32;
            ble_status(session, 0x00U, 0U, now_ms, 1U);
        }
    } else if (p[0] == 0xC2U) {
        if (command.size < 4U || p[3] > 8U || command.size != 4U + 2U * p[3]) {
            ble_count(g_ble_diag.command_errors);
            ble_status(requested, 0x82U, command.size, now_ms);
            return;
        }
        uint16_t packet_count = (uint16_t)((events.count + BLE_SAMPLES_PER_PACKET - 1U) /
                                           BLE_SAMPLES_PER_PACKET);
        for (uint8_t i = 0U; i < p[3]; i++) {
            uint16_t index = ble_read_u16(p + 4U + 2U * i);
            if (index >= packet_count) {
                ble_count(g_ble_diag.command_errors);
                ble_status(requested, 0x81U, index, now_ms);
                return;
            }
            g_reliable_retry_indexes[i] = index;
        }
        g_reliable_retry_count = p[3];
        g_reliable_retry_cursor = 0U;
        ble_status(session, 0x01U, p[3], now_ms, 2U);
    } else if (p[0] == 0xC3U && command.size == 3U) {
        uint16_t packet_count = (uint16_t)((events.count + BLE_SAMPLES_PER_PACKET - 1U) /
                                           BLE_SAMPLES_PER_PACKET);
        ble_status(session, 0x02U, packet_count, now_ms, 3U);
    } else if (p[0] == 0xC4U && command.size == 3U) {
        ble_status(session, 0x03U, 0U, now_ms, 4U);
    } else {
        ble_count(g_ble_diag.command_errors);
        ble_status(requested, 0x82U, command.size, now_ms);
    }
}

static bool ble_notify_packet(const ble_link_t &link, const uint8_t *packet, size_t size) {
    ble_link_t latest = ble_get_link();
    if (!latest.connected || !latest.notify_subscribed || latest.session != link.session) {
        return false;
    }
    bool sent = g_ble_notify->notify(packet, size, link.handle);
    if (!sent) {
        ble_count(g_ble_diag.notify_failures);
    }
    return sent;
}

static void ble_process_commands(const ble_link_t &link, const brd_record_info_t &record,
                                 uint32_t now_ms) {
    if (g_reliable_status.pending) {
        return;
    }
    for (uint8_t i = 0U; i < BLE_COMMAND_QUEUE_SIZE - 1U; i++) {
        ble_command_t command;
        if (!ble_pop_command(command)) {
            break;
        }
        if (command.session != link.session) {
            ble_count(g_ble_diag.command_errors);
            continue;
        }
        ble_command(command, record, now_ms);
        if (g_reliable_status.pending) {
            break;
        }
    }
}

static void ble_update_control(const brd_record_info_t &record) {
    uint8_t value[20] = {};
    brd_event_info_t events = brd_record_get_event_info();
    value[0] = 4U; /* Reliable Protocol V4 */
    value[1] = (uint8_t)g_ble_diag.tx_state;
    value[2] = (record.ready ? 1U : 0U) | (events.truncated ? 2U : 0U);
    value[3] = g_ble_diag.reliable_mode ? 1U : 0U;
    ble_u16(value + 4U, record.ready ? ble_protocol_session(record) : 0U);
    ble_u16(value + 6U, events.count);
    ble_u32(value + 8U, events.crc32);
    ble_u16(value + 12U, events.duration_ms);
    brd_ble_diagnostics_t diag = brd_ble_get_diagnostics();
    ble_u16(value + 14U, (uint16_t)(diag.command_errors > UINT16_MAX ? UINT16_MAX : diag.command_errors));
    ble_u16(value + 16U, (uint16_t)(diag.notify_failures > UINT16_MAX ? UINT16_MAX : diag.notify_failures));
    ble_u16(value + 18U, (uint16_t)(diag.ack_timeouts > UINT16_MAX ? UINT16_MAX : diag.ack_timeouts));
    portENTER_CRITICAL(&g_ble_mux);
    memcpy(g_ble_control_value, value, sizeof(value));
    portEXIT_CRITICAL(&g_ble_mux);
}

static void ble_reliable_update(const ble_link_t &link, const brd_record_info_t &record,
                                uint32_t now_ms) {
    if (!link.notify_subscribed || (uint32_t)(now_ms - g_ble_subscribe_ms) < BLE_SUBSCRIBE_SETTLE_MS) {
        return;
    }

    /* [V1.11 新增] A4 無論成功與否最多等待固定時間，逾時執行既定恢復動作。 */
    if (g_reliable_status.pending &&
        (uint32_t)(now_ms - g_reliable_status.start_ms) >= BLE_STATUS_TIMEOUT_MS) {
        uint8_t after = g_reliable_status.after;
        g_reliable_status = {};
        ble_count(g_ble_diag.status_timeouts);
        ble_apply_status_after(after, now_ms);
    }

    if (record.ready && g_ble_diag.tx_state == BRD_BLE_TX_IDLE && !g_reliable_status.pending) {
        ble_start_transfer(now_ms, true);
    }

    bool transmitting = g_ble_diag.tx_state >= BRD_BLE_TX_START &&
                        g_ble_diag.tx_state <= BRD_BLE_TX_END;
    if (transmitting && !g_ble_ack_clock_started &&
        (uint32_t)(now_ms - g_ble_tx_start_ms) >= BLE_SEND_TIMEOUT_MS) {
        g_ble_diag.tx_state = BRD_BLE_TX_TIMEOUT;
        ble_count(g_ble_diag.send_timeouts);
    } else if (g_ble_ack_clock_started && g_ble_diag.tx_state != BRD_BLE_TX_DONE &&
               g_ble_diag.tx_state != BRD_BLE_TX_TIMEOUT &&
               (uint32_t)(now_ms - g_ble_ack_start_ms) >= BLE_ACK_TIMEOUT_MS) {
        g_ble_diag.tx_state = BRD_BLE_TX_TIMEOUT;
        ble_count(g_ble_diag.ack_timeouts);
    }

    if (g_ble_diag.tx_state == BRD_BLE_TX_WAIT_ACK &&
        (uint32_t)(now_ms - g_ble_last_end_ms) >= BLE_ACK_RETRY_MS &&
        !g_reliable_status.pending) {
        ble_start_transfer(now_ms, false);
        ble_count(g_ble_diag.retransmissions);
    }

    if ((uint32_t)(now_ms - g_ble_last_packet_ms) < BLE_PACKET_INTERVAL_MS) {
        return;
    }

    brd_event_info_t events = brd_record_get_event_info();
    brd_telemetry_t telemetry = brd_measurement_get_telemetry();
    bool pending = record.ready && g_ble_diag.tx_state != BRD_BLE_TX_DONE;
    uint8_t state = pending ? 4U : (uint8_t)telemetry.state;
    uint8_t flags = (telemetry.loaded ? 1U : 0U) | (telemetry.active ? 2U : 0U) |
                    (record.launch_valid ? 4U : 0U) | (pending ? 8U : 0U) |
                    (brd_io_is_charging() ? 16U : 0U) |
                    (g_ble_diag.tx_state == BRD_BLE_TX_WAIT_ACK ? 32U : 0U);
    uint8_t packet[20] = {};
    uint8_t kind = 0U;
    size_t size = 0U;

    if (g_reliable_status.pending) {
        kind = 1U;
        packet[0] = 0xA4U;
        ble_u16(packet + 1U, g_reliable_status.session);
        packet[3] = g_reliable_status.code;
        ble_u16(packet + 4U, g_reliable_status.detail);
        size = 6U;
    } else if (!g_ble_have_live ||
               (uint32_t)(now_ms - g_ble_last_live_ms) >= BLE_LIVE_INTERVAL_MS) {
        kind = 2U;
        packet[0] = 0xB1U;
        packet[1] = state;
        packet[2] = flags;
        ble_u16(packet + 3U, telemetry.current_rpm);
        ble_u16(packet + 5U, telemetry.max_rpm);
        ble_u16(packet + 7U, record.launch_rpm);
        ble_u16(packet + 9U, events.duration_ms);
        ble_u16(packet + 11U, events.count);
        size = 13U;
    } else if (record.launch_valid && !g_ble_launch_sent) {
        kind = 3U;
        packet[0] = 0xB2U;
        ble_u16(packet + 1U, record.launch_rpm);
        ble_u16(packet + 3U, record.max_at_launch);
        ble_u16(packet + 5U, record.launch_time_ms);
        ble_u16(packet + 7U, events.launch_sample_index);
        size = 9U;
    } else if (g_ble_diag.tx_state == BRD_BLE_TX_START) {
        kind = 4U;
        packet[0] = 0xA1U;
        ble_u16(packet + 1U, events.count);
        ble_u16(packet + 3U, 0U); /* event curve: 非固定週期 */
        ble_u16(packet + 5U, events.duration_ms);
        ble_u16(packet + 7U, record.max_rpm);
        ble_u16(packet + 9U, record.launch_rpm);
        ble_u16(packet + 11U, record.launch_time_ms);
        ble_u16(packet + 13U, events.launch_sample_index);
        packet[15] = (record.launch_valid ? 1U : 0U) |
                     (events.truncated ? 0U : 2U) | 4U;
        ble_u16(packet + 16U, events.max_time_ms);
        ble_u16(packet + 18U, ble_protocol_session(record));
        size = 20U;
    } else if (g_ble_diag.tx_state == BRD_BLE_TX_DATA) {
        uint16_t packet_index = g_reliable_retrying ?
            g_reliable_retry_indexes[g_reliable_retry_cursor] : g_reliable_packet_index;
        uint16_t start = (uint16_t)(packet_index * BLE_SAMPLES_PER_PACKET);
        if (start >= events.count) {
            g_ble_diag.tx_state = BRD_BLE_TX_END;
            return;
        }
        kind = 5U;
        packet[0] = 0xA2U;
        ble_u16(packet + 2U, packet_index);
        while (packet[1] < BLE_SAMPLES_PER_PACKET && start + packet[1] < events.count) {
            brd_sample_t sample;
            if (!brd_record_get_event_sample((uint16_t)(start + packet[1]), sample)) {
                return;
            }
            ble_u16(packet + 4U + packet[1] * 4U, sample.time_ms);
            ble_u16(packet + 6U + packet[1] * 4U, sample.rpm);
            packet[1]++;
        }
        size = 4U + packet[1] * 4U;
    } else if (g_ble_diag.tx_state == BRD_BLE_TX_END) {
        kind = 6U;
        packet[0] = 0xA3U;
        ble_u16(packet + 1U, ble_protocol_session(record));
        ble_u16(packet + 3U, (uint16_t)((events.count + BLE_SAMPLES_PER_PACKET - 1U) /
                                        BLE_SAMPLES_PER_PACKET));
        ble_u16(packet + 5U, events.count);
        ble_u32(packet + 7U, events.crc32);
        size = 11U;
    }

    if (size == 0U) {
        return;
    }
    g_ble_last_packet_ms = now_ms;
    if (!ble_notify_packet(link, packet, size)) {
        return;
    }

    if (kind == 1U) {
        uint8_t after = g_reliable_status.after;
        g_reliable_status = {};
        ble_apply_status_after(after, now_ms);
    } else if (kind == 2U) {
        g_ble_have_live = true;
        g_ble_last_live_ms = now_ms;
        g_ble_notify->setValue(packet, size);
    } else if (kind == 3U) {
        g_ble_launch_sent = true;
    } else if (kind == 4U) {
        g_reliable_packet_index = 0U;
        g_reliable_retrying = false;
        g_ble_diag.tx_state = BRD_BLE_TX_DATA;
    } else if (kind == 5U) {
        if (g_reliable_retrying) {
            if (++g_reliable_retry_cursor >= g_reliable_retry_count) {
                g_reliable_retrying = false;
                g_ble_diag.tx_state = BRD_BLE_TX_END;
            }
        } else if (++g_reliable_packet_index >=
                   (events.count + BLE_SAMPLES_PER_PACKET - 1U) / BLE_SAMPLES_PER_PACKET) {
            g_ble_diag.tx_state = BRD_BLE_TX_END;
        }
    } else if (kind == 6U) {
        g_ble_last_end_ms = now_ms;
        g_ble_diag.tx_state = BRD_BLE_TX_WAIT_ACK;
        if (!g_ble_ack_clock_started) {
            g_ble_ack_clock_started = true;
            g_ble_ack_start_ms = now_ms;
        }
    }
}

void brd_ble_update(void) {
    if (!g_ble_diag.initialized) {
        return;
    }
    uint32_t now_ms = millis();
    ble_link_t link = ble_get_link();
    brd_record_info_t record = brd_record_get_info();
    g_ble_diag.connected = link.connected;
    g_ble_diag.subscribed = link.notify_subscribed;
    g_ble_diag.reliable_mode = true;

    if (record.epoch != g_ble_epoch_seen) {
        g_ble_epoch_seen = record.epoch;
        ble_reset_transfer();
        g_ble_have_live = false;
        g_reliable_status = {};
    }
    if (link.session != g_ble_session_seen) {
        g_ble_session_seen = link.session;
        g_ble_subscribe_ms = now_ms;
        g_ble_have_live = false;
        g_reliable_status = {};
        if (g_ble_diag.tx_state != BRD_BLE_TX_DONE) {
            ble_reset_transfer();
        }
        g_ble_last_adv_ms = now_ms - BLE_INIT_RETRY_MS;
    }

    ble_process_commands(link, record, now_ms);
    record = brd_record_get_info();

    if (!link.connected) {
        if (!g_ble_advertising->isAdvertising() &&
            (uint32_t)(now_ms - g_ble_last_adv_ms) >= BLE_INIT_RETRY_MS) {
            g_ble_last_adv_ms = now_ms;
            g_ble_advertising->start();
        }
        ble_update_control(record);
        return;
    }

    ble_reliable_update(link, record, now_ms);
    ble_update_control(brd_record_get_info());
}

brd_ble_diagnostics_t brd_ble_get_diagnostics(void) {
    brd_ble_diagnostics_t result = g_ble_diag;
    portENTER_CRITICAL(&g_ble_mux);
    uint32_t callback_errors = g_ble_callback_errors;
    result.connected = g_ble_link.connected;
    result.subscribed = g_ble_link.notify_subscribed;
    portEXIT_CRITICAL(&g_ble_mux);
    result.command_errors = UINT32_MAX - result.command_errors < callback_errors ?
                            UINT32_MAX : result.command_errors + callback_errors;
    return result;
}

const char *brd_ble_get_device_name(void) {
    return g_ble_name;
}

bool brd_ble_stack_stopped(void) {
    return !NimBLEDevice::isInitialized();
}
