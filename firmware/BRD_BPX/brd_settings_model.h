/*
    檔案位置: BRD_BPX/brd_settings_model.h
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [新增] 四項設定、RAM 草稿、保存狀態與固定長度 BLE / NVS 編碼。
    本檔不依賴 Arduino，所有操作由主 loop 執行。
*/
#ifndef BRD_SETTINGS_MODEL_H
#define BRD_SETTINGS_MODEL_H
#include <stddef.h>
#include <stdint.h>

namespace brd_settings {
constexpr size_t REQUEST_SIZE = 8U;
constexpr size_t RESPONSE_SIZE = 20U;
constexpr size_t STORAGE_SIZE = 16U;
enum Parameter : uint8_t {
    PARAM_MODE = 1U,
    PARAM_AUTO_MIN_RPM = 2U,
    PARAM_FINISH_PERCENT = 3U,
    PARAM_CHARGING_BLE_OFF = 4U
};
enum Command : uint8_t {
    CMD_GET = 1U, CMD_SET = 2U, CMD_SAVE = 3U,
    CMD_DEFAULTS = 4U, CMD_DISCARD = 5U
};
enum Status : uint8_t {
    OK = 0U, PENDING = 1U, SAVED = 2U,
    BAD_LENGTH = 3U, BAD_VERSION = 4U, BAD_COMMAND = 5U,
    BAD_PARAMETER = 6U, OUT_OF_RANGE = 7U, BUSY = 8U,
    FLASH_ERROR = 9U, BAD_MAGIC = 10U
};
struct Values {
    uint8_t mode;
    uint8_t charging_ble_off;
    uint8_t finish_percent;
    uint16_t auto_min_rpm;
};
bool equal(const Values &first, const Values &second);
bool valid(const Values &value, uint32_t rpm_limit);
void encode_storage(const Values &value, uint8_t out[STORAGE_SIZE]);
bool decode_storage(const uint8_t *data, size_t length, uint32_t rpm_limit, Values &out);

class Manager {
public:
    Manager(const Values &defaults, uint32_t rpm_limit);
    const Values &active() const;
    const Values &staged() const;
    const Values &pending_values() const;
    void restore(const Values &value, bool storage_valid);
    Status stage(uint8_t parameter, uint32_t value);
    Status stage_defaults();
    Status discard_staged();
    Status request_save(uint8_t sequence);
    void complete_save(bool success);
    bool pending() const;
    bool dirty() const;
    bool storage_fault() const;
    uint8_t save_sequence() const;
    Status last_save_status() const;
    uint16_t revision() const;
private:
    Values defaults_, active_, staged_, pending_values_;
    uint32_t rpm_limit_;
    uint16_t revision_;
    uint8_t save_sequence_;
    Status last_save_status_;
    bool pending_, storage_valid_;
};

void make_response(const Manager &model, uint8_t command, uint8_t sequence,
                   uint8_t parameter, Status status, uint8_t out[RESPONSE_SIZE]);
Status handle_request(Manager &model, const uint8_t *data, size_t length,
                      bool modification_allowed, uint8_t out[RESPONSE_SIZE]);
} // namespace brd_settings
#endif
