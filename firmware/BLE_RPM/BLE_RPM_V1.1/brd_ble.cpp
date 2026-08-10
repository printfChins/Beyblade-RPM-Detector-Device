/*
    檔案名稱: brd_ble.cpp
    檔案位置: BLE_RPM_V1.0/brd_ble.cpp


    BLE 封包格式:
        0xB1: LIVE
        0xB2: LAUNCH
        0xA1: CURVE START
        0xA2: CURVE DATA
        0xA3: CURVE END
*/

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "brd_ble.h"
#include "brd_config.h"
#include "brd_context.h"
#include "brd_measurement.h"
#include "brd_utils.h"

static NimBLEServer *g_ble_server = nullptr;
static NimBLECharacteristic *g_notify_char = nullptr;

static bool ble_notify_packet(const uint8_t *packet, uint8_t len, bool add_delay);
static uint8_t ble_make_status_flags(void);
static bool ble_send_curve_start(void);
static bool ble_send_curve_data(void);
static bool ble_send_curve_end(void);

class RpmServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
        brd_context_t &ctx = brd_context_get();

        ctx.ble_connected = true;
        ctx.last_live_notify_ms = 0;

        pServer->updateConnParams(connInfo.getConnHandle(), 12, 24, 0, 400);

#if (LOG_MASTER && LOG_BLE)
        Serial.print("BLE connected: ");
        Serial.println(connInfo.getAddress().toString().c_str());
#endif
    }

    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
        brd_context_t &ctx = brd_context_get();

        (void)pServer;
        (void)connInfo;
        (void)reason;

        ctx.ble_connected = false;
        NimBLEDevice::startAdvertising();

#if (LOG_MASTER && LOG_BLE)
        Serial.println("BLE disconnected");
        Serial.println("BLE advertising restarted");
#endif
    }
};

void brd_ble_make_device_name(void) {
    brd_context_t &ctx = brd_context_get();
    uint64_t chip_id;
    uint16_t suffix;

    chip_id = ESP.getEfuseMac();
    suffix = (uint16_t)(chip_id & 0xFFFFULL);

    snprintf(ctx.ble_device_name,
             sizeof(ctx.ble_device_name),
             "%s_%04X",
             PROJECT_SHORT_NAME,
             suffix);
}

void brd_ble_init(void) {
    brd_context_t &ctx = brd_context_get();
    NimBLEService *brd_service;
    NimBLEAdvertising *advertising;

    NimBLEDevice::init(ctx.ble_device_name);
    NimBLEDevice::setPower(ESP_PWR_LVL_N6);
    NimBLEDevice::setMTU(23);

    g_ble_server = NimBLEDevice::createServer();
    g_ble_server->setCallbacks(new RpmServerCallbacks());

    brd_service = g_ble_server->createService(BRD_SERVICE_UUID);

    g_notify_char = brd_service->createCharacteristic(
        BRD_NOTIFY_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    {
        uint8_t packet[1];

        packet[0] = BLE_PKT_CURVE_END;
        g_notify_char->setValue(packet, sizeof(packet));
    }

    brd_service->start();

    advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BRD_SERVICE_UUID);
    advertising->setName(ctx.ble_device_name);
    advertising->setMinInterval(160);
    advertising->setMaxInterval(800);
    advertising->start();

#if (LOG_MASTER && LOG_BLE)
    Serial.print("BRD BLE advertising started, name=");
    Serial.println(ctx.ble_device_name);
#endif
}

static bool ble_notify_packet(const uint8_t *packet, uint8_t len, bool add_delay) {
    brd_context_t &ctx = brd_context_get();

    if ((g_notify_char == nullptr) || (ctx.ble_connected == false)) {
        return false;
    }

    g_notify_char->setValue(packet, len);
    g_notify_char->notify();

    if (add_delay == true) {
        delay(BLE_PACKET_DELAY_MS);
    }

    return true;
}

static uint8_t ble_make_status_flags(void) {
    brd_context_t &ctx = brd_context_get();
    uint8_t flags;

    flags = 0U;

    if (brd_load_is_active() == true) {
        flags |= (1U << 0);
    }

    if (brd_measurement_is_active() == true) {
        flags |= (1U << 1);
    }

    if (ctx.launch_mark_valid == true) {
        flags |= (1U << 2);
    }

    if (ctx.brd_state == BRD_STATE_RESULT_PENDING) {
        flags |= (1U << 3);
    }

    if (ctx.charging == true) {
        flags |= (1U << 4);
    }

    return flags;
}

void brd_ble_send_live_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint8_t packet[13];
    uint32_t now_ms;

    if (ctx.ble_connected == false) {
        return;
    }

    now_ms = millis();

    if ((ctx.last_live_notify_ms != 0UL) &&
        ((uint32_t)(now_ms - ctx.last_live_notify_ms) < BLE_LIVE_INTERVAL_MS)) {
        return;
    }

    ctx.last_live_notify_ms = now_ms;

    packet[0] = BLE_PKT_LIVE;
    packet[1] = (uint8_t)ctx.brd_state;
    packet[2] = ble_make_status_flags();

    brd_write_u16_le(&packet[3], ctx.current_rpm);
    brd_write_u16_le(&packet[5], ctx.max_rpm);
    brd_write_u16_le(&packet[7], ctx.launch_rpm);
    brd_write_u16_le(&packet[9], brd_measurement_elapsed_ms());
    brd_write_u16_le(&packet[11], ctx.curve_count);

    ble_notify_packet(packet, sizeof(packet), false);
}

void brd_ble_send_launch_event_task(void) {
    brd_context_t &ctx = brd_context_get();
    uint8_t packet[9];

    if (ctx.launch_event_pending == false) {
        return;
    }

    if (ctx.ble_connected == false) {
        return;
    }

    packet[0] = BLE_PKT_LAUNCH;
    brd_write_u16_le(&packet[1], ctx.launch_rpm);
    brd_write_u16_le(&packet[3], ctx.max_rpm);
    brd_write_u16_le(&packet[5], ctx.launch_time_ms);
    brd_write_u16_le(&packet[7], ctx.launch_sample_index);

    if (ble_notify_packet(packet, sizeof(packet), false) == true) {
        ctx.launch_event_pending = false;

#if (LOG_MASTER && LOG_BLE)
        Serial.println("BRD BLE launch event sent");
#endif
    }
}

static bool ble_send_curve_start(void) {
    brd_context_t &ctx = brd_context_get();
    uint8_t packet[16];
    uint16_t duration_ms;
    uint8_t flags;

    if (ctx.curve_count == 0U) {
        duration_ms = 0U;
    } else {
        duration_ms = ctx.curve_buffer[ctx.curve_count - 1U].time_ms;
    }

    flags = 0U;

    if (ctx.launch_mark_valid == true) {
        flags |= (1U << 0);
    }

    packet[0] = BLE_PKT_CURVE_START;
    brd_write_u16_le(&packet[1], ctx.curve_count);
    brd_write_u16_le(&packet[3], (uint16_t)RPM_SAMPLE_INTERVAL_MS);
    brd_write_u16_le(&packet[5], duration_ms);
    brd_write_u16_le(&packet[7], ctx.max_rpm);
    brd_write_u16_le(&packet[9], ctx.launch_rpm);
    brd_write_u16_le(&packet[11], ctx.launch_time_ms);
    brd_write_u16_le(&packet[13], ctx.launch_sample_index);
    packet[15] = flags;

    return ble_notify_packet(packet, sizeof(packet), true);
}

static bool ble_send_curve_data(void) {
    brd_context_t &ctx = brd_context_get();
    uint8_t packet[20];
    uint16_t index;
    uint8_t count;
    uint8_t pos;

    index = 0U;

    while (index < ctx.curve_count) {
        count = 0U;
        pos = 2U;

        packet[0] = BLE_PKT_CURVE_DATA;

        while ((count < BLE_SAMPLES_PER_PACKET) &&
               (index < ctx.curve_count)) {
            brd_write_u16_le(&packet[pos], ctx.curve_buffer[index].time_ms);
            pos += 2U;

            brd_write_u16_le(&packet[pos], ctx.curve_buffer[index].rpm);
            pos += 2U;

            count++;
            index++;
        }

        packet[1] = count;

        if (ble_notify_packet(packet,
                              (uint8_t)(2U + (count * 4U)),
                              true) == false) {
            return false;
        }
    }

    return true;
}

static bool ble_send_curve_end(void) {
    uint8_t packet[1];

    packet[0] = BLE_PKT_CURVE_END;

    return ble_notify_packet(packet, sizeof(packet), true);
}

void brd_ble_send_result_task(void) {
    brd_context_t &ctx = brd_context_get();

    if (ctx.brd_state != BRD_STATE_RESULT_PENDING) {
        return;
    }

    /*
        BLE 未連線時保留結果，不清除曲線。
        BLE 重新連線後再傳送完整結果。
    */
    if (ctx.ble_connected == false) {
        return;
    }

#if (LOG_MASTER && LOG_BLE)
    Serial.print("BRD BLE send result: samples=");
    Serial.print(ctx.curve_count);
    Serial.print(" max_rpm=");
    Serial.print(ctx.max_rpm);
    Serial.print(" launch_rpm=");
    Serial.println(ctx.launch_rpm);
#endif

    if (ble_send_curve_start() == false) {
        return;
    }

    if (ble_send_curve_data() == false) {
        return;
    }

    if (ble_send_curve_end() == false) {
        return;
    }

#if (LOG_MASTER && LOG_BLE)
    Serial.println("BRD BLE curve result sent");
#endif

    brd_measurement_reset_after_result_send();
}
