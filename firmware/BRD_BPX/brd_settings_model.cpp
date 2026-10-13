/*
    檔案位置: BRD_BPX/brd_settings_model.cpp
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [新增] 只處理設定邏輯，不在本模組寫入 Flash 或操作 BLE。
    [新增] 保存失敗維持舊執行設定；NVS 使用明確序列化，避免 struct padding。
*/
#include "brd_settings_model.h"
namespace brd_settings {
static uint32_t crc32(const uint8_t *data, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0U; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
        }
    }
    return ~crc;
}
static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value);
    out[1] = static_cast<uint8_t>(value >> 8U);
}
bool equal(const Values &first, const Values &second) {
    return first.mode == second.mode && first.charging_ble_off == second.charging_ble_off &&
           first.finish_percent == second.finish_percent && first.auto_min_rpm == second.auto_min_rpm;
}
bool valid(const Values &value, uint32_t rpm_limit) {
    return value.mode <= 1U && value.charging_ble_off <= 1U &&
           value.finish_percent >= 1U && value.finish_percent <= 100U &&
           value.auto_min_rpm >= 1U && value.auto_min_rpm < rpm_limit;
}
void encode_storage(const Values &value, uint8_t out[STORAGE_SIZE]) {
    out[0] = 'B'; out[1] = 'R'; out[2] = 'D'; out[3] = 'C';
    out[4] = 1U; out[5] = value.mode; out[6] = value.charging_ble_off;
    out[7] = value.finish_percent;
    put_u16(out + 8U, value.auto_min_rpm);
    out[10] = out[11] = 0U;
    uint32_t crc = crc32(out, 12U);
    for (uint8_t index = 0U; index < 4U; index++) {
        out[12U + index] = static_cast<uint8_t>(crc >> (8U * index));
    }
}
bool decode_storage(const uint8_t *data, size_t length, uint32_t rpm_limit, Values &out) {
    if (data == nullptr || length != STORAGE_SIZE ||
        data[0] != 'B' || data[1] != 'R' || data[2] != 'D' || data[3] != 'C' ||
        data[4] != 1U || data[10] != 0U || data[11] != 0U) {
        return false;
    }
    uint32_t stored_crc = 0U;
    for (uint8_t index = 0U; index < 4U; index++) {
        stored_crc |= static_cast<uint32_t>(data[12U + index]) << (8U * index);
    }
    Values value = {data[5], data[6], data[7],
                    static_cast<uint16_t>(data[8] | (static_cast<uint16_t>(data[9]) << 8U))};
    if (stored_crc != crc32(data, 12U) || !valid(value, rpm_limit)) {
        return false;
    }
    out = value;
    return true;
}
Manager::Manager(const Values &defaults, uint32_t rpm_limit)
    : defaults_(defaults), active_(defaults), staged_(defaults), pending_values_(defaults),
      rpm_limit_(rpm_limit), revision_(0U), save_sequence_(0U), last_save_status_(OK),
      pending_(false), storage_valid_(false) {}
const Values &Manager::active() const { return active_; }
const Values &Manager::staged() const { return staged_; }
const Values &Manager::pending_values() const { return pending_values_; }
void Manager::restore(const Values &value, bool storage_valid) {
    active_ = staged_ = pending_values_ = valid(value, rpm_limit_) ? value : defaults_;
    pending_ = false;
    storage_valid_ = storage_valid && valid(value, rpm_limit_);
    last_save_status_ = storage_valid_ ? SAVED : OK;
}
Status Manager::stage(uint8_t parameter, uint32_t value) {
    if (pending_) { return BUSY; }
    Values next = staged_;
    switch (parameter) {
        case PARAM_MODE:
            if (value > 1U) { return OUT_OF_RANGE; }
            next.mode = static_cast<uint8_t>(value);
            break;
        case PARAM_CHARGING_BLE_OFF:
            if (value > 1U) { return OUT_OF_RANGE; }
            next.charging_ble_off = static_cast<uint8_t>(value);
            break;
        case PARAM_FINISH_PERCENT:
            if (value < 1U || value > 100U) { return OUT_OF_RANGE; }
            next.finish_percent = static_cast<uint8_t>(value);
            break;
        case PARAM_AUTO_MIN_RPM:
            if (value < 1U || value >= rpm_limit_ || value > UINT16_MAX) { return OUT_OF_RANGE; }
            next.auto_min_rpm = static_cast<uint16_t>(value);
            break;
        default: return BAD_PARAMETER;
    }
    staged_ = next;
    return OK;
}
Status Manager::stage_defaults() {
    if (pending_) { return BUSY; }
    staged_ = defaults_;
    return OK;
}
Status Manager::discard_staged() {
    if (pending_) { return BUSY; }
    staged_ = active_;
    return OK;
}
Status Manager::request_save(uint8_t sequence) {
    if (pending_) { return BUSY; }
    save_sequence_ = sequence;
    if (storage_valid_ && equal(staged_, active_)) {
        last_save_status_ = SAVED;
        return SAVED;
    }
    pending_values_ = staged_;
    pending_ = true;
    last_save_status_ = PENDING;
    return PENDING;
}
void Manager::complete_save(bool success) {
    if (!pending_) { return; }
    if (success) {
        active_ = staged_ = pending_values_;
        storage_valid_ = true;
        revision_++;
    }
    last_save_status_ = success ? SAVED : FLASH_ERROR;
    pending_ = false;
}
bool Manager::pending() const { return pending_; }
bool Manager::dirty() const { return !equal(staged_, active_); }
bool Manager::storage_fault() const { return !storage_valid_ || last_save_status_ == FLASH_ERROR; }
uint8_t Manager::save_sequence() const { return save_sequence_; }
Status Manager::last_save_status() const { return last_save_status_; }
uint16_t Manager::revision() const { return revision_; }

void make_response(const Manager &model, uint8_t command, uint8_t sequence,
                   uint8_t parameter, Status status, uint8_t out[RESPONSE_SIZE]) {
    out[0] = 0xC2U; out[1] = 1U; out[2] = command; out[3] = sequence;
    out[4] = status; out[5] = parameter;
    out[6] = (model.pending() ? 1U : 0U) | (model.dirty() ? 2U : 0U) |
             (model.storage_fault() ? 4U : 0U);
    out[7] = model.active().mode;
    put_u16(out + 8U, model.active().auto_min_rpm);
    out[10] = model.active().charging_ble_off;
    out[11] = model.staged().mode;
    put_u16(out + 12U, model.staged().auto_min_rpm);
    out[14] = model.staged().charging_ble_off;
    out[15] = model.active().finish_percent;
    out[16] = model.staged().finish_percent;
    out[17] = model.last_save_status();
    put_u16(out + 18U, model.revision());
}
Status handle_request(Manager &model, const uint8_t *data, size_t length,
                      bool modification_allowed, uint8_t out[RESPONSE_SIZE]) {
    uint8_t command = data != nullptr && length > 2U ? data[2] : 0U;
    uint8_t sequence = data != nullptr && length > 3U ? data[3] : 0U;
    uint8_t parameter = data != nullptr && length > 4U ? data[4] : 0U;
    Status status;
    if (data == nullptr || length != REQUEST_SIZE) { status = BAD_LENGTH; }
    else if (data[0] != 0xC1U) { status = BAD_MAGIC; }
    else if (data[1] != 1U) { status = BAD_VERSION; }
    else if (command < CMD_GET || command > CMD_DISCARD) { status = BAD_COMMAND; }
    else if (command != CMD_GET && !modification_allowed) { status = BUSY; }
    else {
        uint32_t value = data[5] | (static_cast<uint32_t>(data[6]) << 8U) |
                         (static_cast<uint32_t>(data[7]) << 16U);
        if (command != CMD_SET && (parameter != 0U || value != 0U)) {
            status = BAD_PARAMETER;
        } else {
            switch (command) {
                case CMD_GET: status = OK; break;
                case CMD_SET: status = model.stage(parameter, value); break;
                case CMD_SAVE: status = model.request_save(sequence); break;
                case CMD_DEFAULTS: status = model.stage_defaults(); break;
                case CMD_DISCARD: status = model.discard_staged(); break;
                default: status = BAD_COMMAND; break;
            }
        }
    }
    make_response(model, command, sequence, parameter, status, out);
    return status;
}
} // namespace brd_settings
