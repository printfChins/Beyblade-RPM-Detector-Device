/* [新增] 直接執行交付模組，覆蓋 BLE 封包、故障、ACK 與事件時間。 */
#define main oled_regression_main
#include "test_v012.cpp"
#undef main

static NimBLEConnInfo peer{1U};

static void connect_peer(bool subscribe = true) {
    brd_ble_set_enabled(true);
    assert(brd_ble_get_diagnostics().initialized);
    g_ble_server->callback->onConnect(g_ble_server, peer);
    NimBLEDevice::advertising.advertising = false;
    if (subscribe) g_ble_notify->callback->onSubscribe(g_ble_notify, peer, 1U);
    brd_ble_update();
}

static void subscribe_peer(bool enabled) {
    g_ble_notify->callback->onSubscribe(g_ble_notify, peer, enabled ? 1U : 0U);
    brd_ble_update();
}

static void write_command(std::vector<uint8_t> bytes) {
    g_ble_control->setValue(bytes.data(), bytes.size());
    g_ble_control->callback->onWrite(g_ble_control, peer);
}

static void command_u32(std::vector<uint8_t> &bytes, uint32_t value) {
    for (unsigned i = 0U; i < 4U; i++) bytes.push_back((uint8_t)(value >> (8U * i)));
}

static void send_ack(uint32_t id, uint32_t crc) {
    std::vector<uint8_t> bytes{0xC1U};
    command_u32(bytes, id); command_u32(bytes, crc);
    write_command(bytes);
    brd_ble_update();
}

static void resend(uint32_t id) {
    std::vector<uint8_t> bytes{0xC2U};
    command_u32(bytes, id); write_command(bytes); brd_ble_update();
}

static size_t packets(uint8_t id) {
    size_t count = 0U;
    for (const auto &packet : mock_notifications) if (packet.bytes[0] == id) count++;
    return count;
}

static size_t indications(uint8_t id) {
    size_t count = 0U;
    for (const auto &packet : mock_indications) if (!packet.bytes.empty() && packet.bytes[0] == id) count++;
    return count;
}

static NimBLECharacteristic *state_characteristic(void) {
    auto &chars = g_ble_server->services[0]->characteristics;
    if (chars.size() < 5U) return nullptr;
    return chars[4].get();
}

static void subscribe_state(bool enabled) {
    NimBLECharacteristic *state = state_characteristic();
    assert(state != nullptr);
    state->callback->onSubscribe(state, peer, enabled ? 2U : 0U);
    brd_ble_update();
}

static void ble_run_ms(uint32_t duration) {
    for (uint32_t i = 0U; i < duration; i++) {
        test_us += 1000;
        size_t before = mock_notifications.size();
        uint64_t before_us = test_us;
        brd_ble_update();
        assert(test_us == before_us); // BLE 工作沒有 delay。
        assert(mock_notifications.size() <= before + 1U);
    }
}

static void complete_shot(uint64_t base) {
    baseline(base); launch(base); pulse_at(base + 24000);
    auto record = brd_record_get_info();
    assert(record.ready && record.count >= 2U && record.max_rpm == 20000U);
}

static void reliable_peer(void) {
    connect_peer();
    assert(brd_ble_get_diagnostics().reliable_mode);
}

// [V1.10 新增] 驗證完整畫面，包含狀態變更於既有更新週期內觸發重畫。
static void finish_oled_frame(void) {
    brd_oled_update();
    for (unsigned i = 0U; g_frame_active && i < 40U; i++) {
        brd_oled_update();
    }
    assert(!g_frame_active);
}

static bool bluetooth_pixels_present(void) {
    for (unsigned x = 119U; x <= 125U; x++) {
        for (unsigned y = 17U; y <= 29U; y++) {
            if (g_oled_buffer[x + (y / 8U) * OLED_WIDTH] & (1U << (y % 8U))) return true;
        }
    }
    return false;
}

// [V1.10 新增] 舊 APP 的 0004 READ 選定 V1.9 傳輸，控制命令送到 0003 WRITE。
static void reliable_protocol_peer(void) {
    connect_peer();
    assert(brd_ble_get_diagnostics().reliable_mode);
}

static void reliable_write(std::vector<uint8_t> bytes) {
    auto *control = g_ble_server->services.front()->characteristics[1].get();
    assert(control->uuid == BRD_CONTROL_CHAR_UUID);
    control->setValue(bytes.data(), bytes.size());
    control->callback->onWrite(control, peer);
    brd_ble_update();
}

static const MockNotification &last_packet(uint8_t kind) {
    for (size_t i = mock_notifications.size(); i != 0U; i--) {
        if (mock_notifications[i - 1U].bytes[0] == kind) return mock_notifications[i - 1U];
    }
    assert(false);
    return mock_notifications.front();
}

int main(int argc, char **argv) {
    assert(argc == 2);
    std::string name = argv[1];
    if (name == "state_event_indication") {
        connect_peer();
        auto &chars = g_ble_server->services[0]->characteristics;
        assert(chars.size() == 5U);
        auto *state = chars[4].get();
        assert(state->uuid == "7f510006-1b15-4d5f-9f4d-9b3c7a1d9a10");
        assert((state->properties & NIMBLE_PROPERTY::READ) != 0U);
        assert((state->properties & NIMBLE_PROPERTY::INDICATE) != 0U);
        assert((state->properties & NIMBLE_PROPERTY::NOTIFY) == 0U);

        subscribe_state(true);
        ble_run_ms(150);
        assert(indications(0x81U) == 1U);
        assert(packets(0xB1U) == 0U);
        auto first = mock_indications.back().bytes;
        assert(first.size() == 5U && first[1] == BRD_STATE_WAIT_LOAD);
        uint16_t first_seq = (uint16_t)(first[3] | (first[4] << 8));

        ble_run_ms(1000);
        assert(indications(0x81U) == 1U);
        assert(packets(0xB1U) == 0U);

        loaded(test_us);
        brd_ble_update();
        ble_run_ms(20);
        assert(indications(0x81U) == 2U);
        auto ready = mock_indications.back().bytes;
        assert(ready[1] == BRD_STATE_LOADED_READY);
        assert((ready[2] & 1U) != 0U);
        uint16_t ready_seq = (uint16_t)(ready[3] | (ready[4] << 8));
        assert((uint16_t)(ready_seq - first_seq) == 1U);

        ble_run_ms(1000);
        assert(indications(0x81U) == 2U);
        assert(packets(0xB1U) == 0U);

        uint64_t spin_base = test_us;
        pulse_at(spin_base + 2000U);
        pulse_at(spin_base + 4000U);
        brd_ble_update();
        ble_run_ms(450);
        assert(indications(0x81U) >= 3U);
        assert(packets(0xB1U) >= 2U);
    } else if (name == "state_indication_timeout") {
        connect_peer();
        mock_indicate_failure = true;
        subscribe_state(true);
        ble_run_ms(2300);
        assert(mock_indicate_attempts > 0U);
        assert(!brd_ble_get_diagnostics().connected);
        assert(brd_ble_get_diagnostics().state_timeouts == 1U);
    } else if (name == "state_confirmation_timeout") {
        connect_peer();
        mock_indicate_no_confirm = true;
        subscribe_state(true);
        ble_run_ms(2300);
        assert(mock_indicate_attempts == 1U);
        assert(!brd_ble_get_diagnostics().connected);
        assert(brd_ble_get_diagnostics().state_timeouts == 1U);
    } else if (name == "state_millis_wrap") {
        test_us = ((1ULL << 32) - 500ULL) * 1000ULL;
        connect_peer();
        mock_indicate_no_confirm = true;
        subscribe_state(true);
        ble_run_ms(2300);
        assert(mock_indicate_attempts == 1U);
        assert(!brd_ble_get_diagnostics().connected);
        assert(brd_ble_get_diagnostics().state_timeouts == 1U);
    } else if (name == "reliable_only_protocol") {
        connect_peer();
        // [V1.11 測試] 唯一協定固定為 Reliable，不需 C0 或讀 0004 切換模式。
        assert(brd_ble_get_diagnostics().reliable_mode);
        auto &chars = g_ble_server->services[0]->characteristics;
        assert(chars.size() == 5U);
        assert((chars[1]->properties & NIMBLE_PROPERTY::WRITE) != 0U);
        assert((chars[1]->properties & NIMBLE_PROPERTY::READ) == 0U);
        assert((chars[2]->properties & NIMBLE_PROPERTY::READ) != 0U);
        assert((chars[2]->properties & NIMBLE_PROPERTY::WRITE) == 0U);
    } else if (name == "reliable_transfer") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(500);
        const auto &start = last_packet(0xA1U).bytes;
        const auto &data = last_packet(0xA2U).bytes;
        const auto &end = last_packet(0xA3U).bytes;
        assert(start.size() == 20U && data.size() == 20U && end.size() == 11U);
        assert(start[1] == 4U && start[3] == 0U && (start[15] & 0x07U) == 0x07U);
        assert(data[1] == 4U && data[2] == 0U && data[3] == 0U);
        // [V1.11 測試] Reliable 曲線第一筆固定為 t=0 / 0 RPM。
        assert(data[4] == 0U && data[5] == 0U && data[6] == 0U && data[7] == 0U);
        assert(end[3] == 1U && end[5] == 4U);
        assert(packets(0xA4U) == 0U && brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_WAIT_ACK);
    } else if (name == "reliable_send_timeout") {
        reliable_protocol_peer(); complete_shot(0);
        mock_notify_failure = true;
        ble_run_ms(BLE_SEND_TIMEOUT_MS + 300U);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_TIMEOUT);
        assert(brd_record_get_info().ready);
    } else if (name == "reliable_status_timeout") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(300);
        auto end = last_packet(0xA3U).bytes;
        uint16_t id = (uint16_t)(end[1] | (end[2] << 8));
        std::vector<uint8_t> ack{0xC1U, (uint8_t)id, (uint8_t)(id >> 8), 4U, 0U,
                                  end[7], end[8], end[9], end[10]};
        mock_notify_failure = true;
        reliable_write(ack);
        ble_run_ms(BLE_STATUS_TIMEOUT_MS + 100U);
        assert(!g_reliable_status.pending);
        assert(!brd_record_get_info().ready);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_DONE ||
               brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_IDLE);
    } else if (name == "reliable_retry_ack") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(300);
        auto end = last_packet(0xA3U).bytes;
        uint16_t id = (uint16_t)(end[1] | (end[2] << 8));
        std::vector<uint8_t> retry{0xC2U, (uint8_t)id, (uint8_t)(id >> 8), 1U, 0U, 0U};
        size_t old_data = packets(0xA2U), old_end = packets(0xA3U);
        reliable_write(retry); ble_run_ms(100);
        assert(last_packet(0xA4U).bytes[3] == 1U);
        assert(packets(0xA2U) == old_data + 1U && packets(0xA3U) == old_end + 1U);
        // [V1.10 新增] C2 count=0 只重送 END，不重傳 A2。
        old_data = packets(0xA2U); old_end = packets(0xA3U);
        reliable_write({0xC2U, (uint8_t)id, (uint8_t)(id >> 8), 0U}); ble_run_ms(35);
        assert(packets(0xA2U) == old_data && packets(0xA3U) == old_end + 1U);
        std::vector<uint8_t> ack{0xC1U, (uint8_t)id, (uint8_t)(id >> 8), 4U, 0U,
                                  end[7], end[8], end[9], end[10]};
        reliable_write(ack); ble_run_ms(30);
        assert(last_packet(0xA4U).bytes[3] == 0U);
        assert(!brd_record_get_info().ready && brd_measurement_get_display().show_max);
        reliable_write(ack); ble_run_ms(30);
        assert(last_packet(0xA4U).bytes[3] == 0U);
    } else if (name == "reliable_restart_abort") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(300);
        uint16_t id = (uint16_t)(last_packet(0xA3U).bytes[1] |
                                   (last_packet(0xA3U).bytes[2] << 8));
        size_t starts = packets(0xA1U);
        reliable_write({0xC3U, 0xFFU, 0xFFU}); ble_run_ms(160);
        assert(last_packet(0xA4U).bytes[3] == 2U && packets(0xA1U) == starts + 1U);
        assert(brd_record_get_info().ready);
        reliable_write({0xC4U, (uint8_t)id, (uint8_t)(id >> 8)}); ble_run_ms(35);
        assert(last_packet(0xA4U).bytes[3] == 3U && !brd_record_get_info().ready);
        assert(brd_measurement_get_display().show_max);
    } else if (name == "reliable_new_load_cancels_ack") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(300);
        auto end = last_packet(0xA3U).bytes;
        uint16_t id = (uint16_t)(end[1] | (end[2] << 8));
        update_at(2600000ULL); // HOLD 期結束，且 ACK 仍在 5 秒期限內。
        auto *legacy = g_ble_server->services.front()->characteristics[1].get();
        std::vector<uint8_t> ack{0xC1U, (uint8_t)id, (uint8_t)(id >> 8), 4U, 0U,
                                 end[7], end[8], end[9], end[10]};
        legacy->setValue(ack.data(), ack.size());
        legacy->callback->onWrite(legacy, peer);
        load_at(test_us + 1ULL, true); update_at(test_us + 1000ULL);
        brd_ble_update(); ble_run_ms(40);
        assert(last_packet(0xA4U).bytes[3] == 0x85U); // 新 LOAD 後舊 ACK 無結果可清。
        assert(!brd_record_get_info().ready && brd_measurement_get_display().loaded);
    } else if (name == "reliable_reconnect_restart") {
        reliable_protocol_peer(); complete_shot(0); ble_run_ms(300);
        auto start = last_packet(0xA1U).bytes; size_t old = packets(0xA1U);
        g_ble_server->disconnect(peer.handle); brd_ble_update();
        connect_peer(false);
        g_ble_firmware->callback->onRead(g_ble_firmware, peer);
        subscribe_peer(true); ble_run_ms(300);
        assert(packets(0xA1U) == old + 1U);
        assert(last_packet(0xA1U).bytes[18] == start[18] && last_packet(0xA1U).bytes[19] == start[19]);
    } else if (name == "timeout_result_blocks_sleep") {
        GPIO.in.val |= 1U << CHRG_DET_GPIO;
        setup();
        brd_record_begin(micros());
        brd_record_finish(micros() + 10000U, 0U, 10000U);
        g_ble_diag.tx_state = BRD_BLE_TX_TIMEOUT;
        g_idle_active = true;
        g_idle_start_ms = millis() - LOW_POWER_DEEP_SLEEP_TIMEOUT_MS;
        brd_power_update();
        assert(mock_sleep_calls == 0);
    } else if (name == "power_idle_sleep") {
        GPIO.in.val |= 1U << CHRG_DET_GPIO; setup(); uint64_t base = test_us;
        for (unsigned i = 1U; i <= 301U; i++) {
            at(base + (uint64_t)i * 1000000ULL);
            brd_battery_update(); brd_measurement_update(); brd_ble_update();
            brd_power_update(); brd_oled_update();
            if (i == 29U) assert(!brd_oled_idle_is_off());
            if (i == 31U) assert(brd_oled_idle_is_off());
        }
        assert(mock_sleep_calls == 1 && mock_sleep_wake_mask == (1ULL << LOAD_IR_GPIO));
        assert(!NimBLEDevice::isInitialized());
    } else if (name == "power_sleep_shutdown_failure") {
        GPIO.in.val |= 1U << CHRG_DET_GPIO; setup();
        brd_oled_set_idle_off(true); brd_oled_update();
        assert(brd_oled_idle_is_off());
        g_idle_active = true;
        g_idle_start_ms = millis() - LOW_POWER_DEEP_SLEEP_TIMEOUT_MS;
        mock_deinit_failure = true;
        brd_power_update();
        assert(mock_sleep_calls == 0 && NimBLEDevice::isInitialized());
        assert(!g_idle_off_requested);
    } else if (name == "power_idle_abort") {
        GPIO.in.val |= 1U << CHRG_DET_GPIO; setup(); uint64_t base = test_us;
        brd_power_update();
        at(base + 30000000ULL); brd_battery_update(); brd_power_update(); brd_oled_update();
        assert(brd_oled_idle_is_off());
        connect_peer(false); brd_power_update(); brd_oled_update();
        assert(!brd_oled_idle_is_off() && mock_sleep_calls == 0);
        g_ble_server->disconnect(peer.handle); brd_ble_update(); brd_power_update();
        at(base + 330000000ULL); brd_battery_update(); brd_power_update();
        assert(mock_sleep_calls == 0);
        // 充電期間也不可累積休眠時間。
        GPIO.in.val &= ~(1U << CHRG_DET_GPIO);
        brd_power_update();
        at(base + 650000000ULL); brd_battery_update(); brd_power_update();
        assert(mock_sleep_calls == 0);
    } else if (name == "oled_connection_icon") {
        setup(); finish_oled_frame(); assert(!bluetooth_pixels_present());
        uint8_t before[sizeof(g_oled_buffer)];
        memcpy(before, g_oled_buffer, sizeof(before));
        connect_peer(false); finish_oled_frame();
        assert(bluetooth_pixels_present() && !brd_ble_get_diagnostics().subscribed);
        // 圖示以外的畫面須完全相同。
        for (unsigned x = 0U; x < 119U; x++) {
            for (unsigned page = 0U; page < 4U; page++) {
                assert(before[x + page * OLED_WIDTH] == g_oled_buffer[x + page * OLED_WIDTH]);
            }
        }
        subscribe_peer(true); finish_oled_frame(); assert(bluetooth_pixels_present());
        subscribe_peer(false); finish_oled_frame(); assert(bluetooth_pixels_present());
        g_ble_server->disconnect(peer.handle); brd_ble_update(); finish_oled_frame();
        assert(!bluetooth_pixels_present());
        connect_peer(false); finish_oled_frame(); assert(bluetooth_pixels_present());
        dump_frame("ble_connected");
    } else if (name == "oled_icon_hold") {
        setup(); brd_measurement_stop(); complete_shot(test_us); finish_oled_frame();
        uint32_t generation = brd_measurement_get_display().generation;
        uint64_t first_frame_us = test_us;
        test_us += 1000000; connect_peer(false); finish_oled_frame();
        assert(bluetooth_pixels_present() && brd_measurement_get_display().hold_active);
        assert(brd_measurement_get_display().generation == generation);
        dump_frame("ble_hold");
        update_at(first_frame_us + 2500000);
        assert(!brd_measurement_get_display().hold_active);
        // 最大合法五位數顯示與圖示共存。
        auto display = brd_measurement_get_display(); display.value = 60000U;
        oled_begin_frame(display, 100U, true, true);
        assert(bluetooth_pixels_present()); dump_frame("ble_max_60000");
    } else if (name == "oled_icon_warning") {
        setup(); connect_peer(false); finish_oled_frame(); assert(bluetooth_pixels_present());
        test_read_error = ESP_ERR_TIMEOUT;
        uint64_t base = test_us;
        for (unsigned i = 1U; i <= 3U; i++) { at(base + i * 1000000ULL); loop(); }
        assert(brd_battery_is_adc_fault() && !brd_ble_get_diagnostics().connected);
        assert(g_frame_is_adc_fault && !bluetooth_pixels_present());
        oled_begin_low_battery_frame(); assert(!bluetooth_pixels_present());
    } else if (name == "offline_record") {
        complete_shot(0);
        ble_run_ms(500);
        assert(mock_notifications.empty());
        assert(brd_measurement_get_display().show_max);
        auto r = brd_record_get_info();
        assert(r.launch_rpm == 20000 && r.launch_time_ms == 6 && r.duration_ms == 21);
    } else if (name == "event_curve_backlog") {
        loaded(); pulse_at(2000, false);
        for (uint64_t t = 4000; t <= 200000; t += 2000) pulse_at(t, false);
        update_at(201000);
        auto r = brd_record_get_info();
        auto e = brd_record_get_event_info();
        assert(r.count == 100U && e.count == 100U && r.duration_ms == 199U);
        brd_sample_t first, second, last;
        assert(brd_record_get_event_sample(0U, first));
        assert(brd_record_get_event_sample(1U, second));
        assert(brd_record_get_event_sample(99U, last));
        assert(first.time_ms == 0U && first.rpm == 0U);
        assert(second.time_ms == 2U && second.rpm == 30000U);
        assert(last.time_ms == 198U && last.rpm == 30000U);
    } else if (name == "launch_raw_snapshot") {
        baseline(); load_at(9500, false); pulse_at(10000, false); update_at(10500);
        auto r = brd_record_get_info();
        assert(r.launch_rpm == 20000 && r.max_at_launch == 20000);
        assert(brd_measurement_get_display().value == 60000);
        assert(r.launch_time_ms == 6 && r.launch_sample_index == 2);
    } else if (name == "curve_cap_and_wrap") {
        brd_record_begin(1000U);
        brd_record_advance(60001000U, 30000U, true);
        assert(brd_record_get_info().count == 1U);
        brd_record_advance(61001000U, 10000U, true);
        brd_record_advance(2000U, 1U, true);
        brd_record_launch(2000U, 1U, 30000U);
        brd_record_finish(3000U, 0U, 30000U);
        auto r = brd_record_get_info();
        assert(r.truncated && r.count == 1U && r.duration_ms == 60000U);
        assert(!r.launch_valid && r.launch_sample_index == CURVE_INVALID_INDEX);
        brd_sample_t sample; assert(!brd_record_get_event_sample(r.count, sample));
    } else if (name == "curve_micros_wrap") {
        uint64_t base = (1ULL << 32) - 10000;
        complete_shot(base);
        auto r = brd_record_get_info();
        assert(r.duration_ms == 21U && r.launch_time_ms == 6U);
    } else if (name == "gatt_identity") {
        connect_peer(false);
        assert(std::string(brd_ble_get_device_name()) == "BRD_ABCD");
        assert(NimBLEDevice::advertising.uuid == BRD_SERVICE_UUID);
        assert(!g_ble_server->auto_advertise);
        auto &chars = g_ble_server->services[0]->characteristics;
        // [V1.10 修改] 0003 READ/WRITE，0004 READ/WRITE，0005 診斷 READ。
        assert(chars.size() == 5);
        assert(chars[1]->uuid == BRD_CONTROL_CHAR_UUID);
        assert((chars[1]->properties & NIMBLE_PROPERTY::WRITE) != 0U);
        assert((chars[1]->properties & NIMBLE_PROPERTY::READ) == 0U);
        assert(chars[2]->uuid == BRD_FIRMWARE_REVISION_CHAR_UUID);
        assert((chars[2]->properties & NIMBLE_PROPERTY::WRITE) == 0U);
        assert(chars[3]->uuid == BRD_DIAGNOSTIC_CHAR_UUID);
        assert(chars[4]->uuid == BRD_STATE_CHAR_UUID);
        assert((chars[4]->properties & NIMBLE_PROPERTY::READ) != 0U);
        assert((chars[4]->properties & NIMBLE_PROPERTY::INDICATE) != 0U);
        auto revision = chars[2]->value.bytes;
        assert(std::string(revision.begin(), revision.end()) == "V1.13");
        ble_run_ms(300); assert(mock_notifications.empty() && mock_indications.empty());
        subscribe_peer(true); ble_run_ms(110); assert(packets(0xB1) == 0U);
        subscribe_state(true); ble_run_ms(110); assert(indications(0x81U) == 1U);
    } else if (name == "legacy_packets") {
        connect_peer(); complete_shot(0); ble_run_ms(800);
        assert(packets(0xA1) == 1 && packets(0xA3) == 1 && packets(0xB2) == 1);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_DONE);
        assert(brd_measurement_get_display().show_max && brd_record_get_info().ready);
        ble_run_ms(6000); assert(packets(0xA1) == 1);
        std::ofstream file("ble_packets.hex");
        const char *hex = "0123456789abcdef";
        for (const auto &packet : mock_notifications) {
            for (auto byte : packet.bytes) file << hex[byte >> 4] << hex[byte & 15];
            file << '\n';
        }
        std::ofstream crc("ble_crc.txt"); crc << brd_record_get_info().crc32 << '\n';
    } else if (name == "live_cadence_and_change") {
        connect_peer(); subscribe_state(true); ble_run_ms(750);
        assert(packets(0xB1) == 0U);
        assert(indications(0x81U) == 1U);
        uint64_t base = test_us; loaded(base); brd_ble_update(); ble_run_ms(20);
        assert(mock_indications.back().bytes[1] == BRD_STATE_LOADED_READY);
        assert((mock_indications.back().bytes[2] & 1U) != 0U);
        size_t state_count = indications(0x81U);
        ble_run_ms(500);
        assert(indications(0x81U) == state_count && packets(0xB1) == 0U);
        uint64_t spin = test_us; pulse_at(spin + 2000U); pulse_at(spin + 4000U);
        brd_ble_update(); ble_run_ms(650);
        assert(packets(0xB1U) >= 3U);
    } else if (name == "notify_failure_retries") {
        connect_peer(); complete_shot(0);
        mock_notify_failure = true; ble_run_ms(300);
        assert(mock_notifications.empty() && brd_ble_get_diagnostics().notify_failures > 0);
        mock_notify_failure = false; ble_run_ms(300);
        assert(packets(0xA1) == 1 && packets(0xA3) == 1);
    } else if (name == "send_timeout") {
        connect_peer(); complete_shot(0); mock_notify_failure = true;
        ble_run_ms(5300);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_TIMEOUT);
        assert(brd_measurement_get_display().value == 20000 && brd_record_get_info().ready);
    } else if (name == "ack_valid_and_bad") {
        reliable_peer(); complete_shot(0); ble_run_ms(500);
        auto r = brd_record_get_info();
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_WAIT_ACK);
        send_ack(r.result_id + 1, r.crc32);
        send_ack(r.result_id, r.crc32 ^ 1U);
        assert(brd_ble_get_diagnostics().command_errors == 2);
        send_ack(r.result_id, r.crc32);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_DONE);
        ble_run_ms(6000); assert(packets(0xA1) == 1);
        assert(brd_measurement_get_display().value == 20000);
    } else if (name == "ack_retry_timeout") {
        reliable_peer(); complete_shot(0); ble_run_ms(5400);
        assert(packets(0xA1) > 1 && brd_ble_get_diagnostics().retransmissions > 0);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_TIMEOUT);
        assert(brd_ble_get_diagnostics().ack_timeouts == 1);
        size_t starts = packets(0xA1); ble_run_ms(1000); assert(packets(0xA1) == starts);
        assert(brd_measurement_get_display().value == 20000);
        resend(brd_record_get_info().result_id); ble_run_ms(400);
        assert(packets(0xA1) == starts + 1);
    } else if (name == "late_ack") {
        reliable_peer(); complete_shot(0); ble_run_ms(500);
        auto r = brd_record_get_info();
        test_us = (uint64_t)(g_ble_ack_start_ms + BLE_ACK_TIMEOUT_MS) * 1000ULL;
        send_ack(r.result_id, r.crc32);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_TIMEOUT);
        assert(brd_ble_get_diagnostics().command_errors == 1);
    } else if (name == "disconnect_mid_curve") {
        connect_peer();
        brd_record_begin(micros()); brd_record_advance(micros() + 10000000, 20000, true);
        brd_record_finish(micros() + 10000000, 4000, 20000);
        ble_run_ms(140); assert(packets(0xA1) == 1 && packets(0xA3) == 0);
        g_ble_server->disconnect(peer.handle); brd_ble_update();
        assert(NimBLEDevice::advertising.advertising);
        connect_peer(); ble_run_ms(1000);
        assert(packets(0xA1) == 2 && packets(0xA3) == 1);
        g_ble_server->disconnect(peer.handle); brd_ble_update();
        connect_peer(); ble_run_ms(1000); assert(packets(0xA1) == 2);
    } else if (name == "unsubscribe_and_stale_command") {
        reliable_peer(); complete_shot(0); ble_run_ms(500);
        auto r = brd_record_get_info();
        std::vector<uint8_t> bytes{0xC1U}; command_u32(bytes, r.result_id); command_u32(bytes, r.crc32);
        write_command(bytes); subscribe_peer(false);
        size_t before = mock_notifications.size(); ble_run_ms(500);
        assert(mock_notifications.size() == before);
        subscribe_peer(true); ble_run_ms(400);
        assert(packets(0xA1) == 2 && !brd_ble_get_diagnostics().reliable_mode);
    } else if (name == "reload_cancels_record") {
        reliable_peer(); complete_shot(0); ble_run_ms(400);
        auto r = brd_record_get_info();
        load_at(500000, true); update_at(2600000); update_at(2601000); brd_ble_update();
        assert(!brd_record_get_info().ready && brd_record_get_info().count == 0);
        assert(brd_measurement_get_display().loaded && brd_measurement_get_display().value == 0);
        send_ack(r.result_id, r.crc32);
        assert(brd_ble_get_diagnostics().command_errors == 1);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_IDLE);
    } else if (name == "command_queue_bounds") {
        connect_peer();
        for (unsigned i = 0; i < 40; i++) write_command({0xCFU});
        brd_ble_update(); ble_run_ms(200);
        assert(brd_ble_get_diagnostics().command_errors == 40);
        // [V1.10 修改] 0004 READ 現為版本；原 20-byte 診斷搬至 0005 READ。
        g_ble_firmware->callback->onRead(g_ble_firmware, peer);
        auto revision = g_ble_firmware->getValue().bytes;
        assert(std::string(revision.begin(), revision.end()) == "V1.13");
        g_ble_diagnostic->callback->onRead(g_ble_diagnostic, peer);
        assert(g_ble_diagnostic->getValue().size() == 20);
    } else if (name == "init_failure_retry") {
        mock_init_failure = true; brd_ble_set_enabled(true);
        assert(!brd_ble_get_diagnostics().initialized && mock_init_calls == 1);
        at(999000); brd_ble_set_enabled(true); assert(mock_init_calls == 1);
        mock_init_failure = false; at(1000000); brd_ble_set_enabled(true);
        assert(brd_ble_get_diagnostics().initialized && mock_init_calls == 2);
    } else if (name == "shutdown_failure_retry") {
        connect_peer(); mock_deinit_failure = true;
        brd_ble_set_enabled(false);
        assert(brd_ble_get_diagnostics().shutdown_failures == 1);
        assert(!brd_ble_get_diagnostics().initialized && NimBLEDevice::isInitialized());
        assert(!NimBLEDevice::advertising.advertising && NimBLEDevice::server);
        at(999000); brd_ble_set_enabled(true); assert(mock_init_calls == 1);
        mock_deinit_failure = false; at(1000000); brd_ble_set_enabled(true);
        assert(brd_ble_get_diagnostics().initialized && mock_init_calls == 2);
    } else if (name == "power_fault_shutdown_resume") {
        setup(); connect_peer(); uint64_t base = test_us;
        for (uint64_t i = 1; i <= 3; i++) {
            at(base + i * 1000000); test_read_error = ESP_ERR_TIMEOUT; loop();
        }
        assert(!brd_ble_get_diagnostics().initialized && !NimBLEDevice::isInitialized());
        assert(!g_measurement_enabled && mock_deinit_calls == 1);
        at(base + 4000000); test_read_error = ESP_OK; loop();
        assert(brd_ble_get_diagnostics().initialized && g_measurement_enabled);
    } else if (name == "low_battery_no_ble") {
        test_adc_mv = 1730; setup();
        assert(!brd_ble_get_diagnostics().initialized && mock_init_calls == 0);
        assert(!g_measurement_enabled);
    } else if (name == "ble_millis_wrap") {
        test_us = ((1ULL << 32) - 300ULL) * 1000ULL;
        reliable_peer(); complete_shot(test_us); ble_run_ms(5500);
        assert(brd_ble_get_diagnostics().tx_state == BRD_BLE_TX_TIMEOUT);
        assert(packets(0xA1) > 1);
    } else {
        return 2;
    }
    std::cout << "PASS " << name << '\n';
    return 0;
}
