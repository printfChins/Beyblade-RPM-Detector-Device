/* [V1.2 新增] 真實 ESP-IDF API 的主機 Flash 邊界，不取代 Session。 */
#ifndef TEST_ESP_OTA_H
#define TEST_ESP_OTA_H
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include "esp_err.h"
using esp_ota_handle_t = uint32_t;
constexpr uint8_t ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10U;
constexpr uint8_t ESP_PARTITION_SUBTYPE_APP_OTA_15 = 0x1FU;
struct esp_partition_t { uint32_t address; uint32_t size; uint8_t subtype; };
inline esp_partition_t test_running_partition = {0x10000U, 0x140000U, 0x10U};
inline esp_partition_t test_next_partition = {0x150000U, 0x140000U, 0x11U};
inline std::vector<uint8_t> test_flash_bytes;
inline unsigned test_flash_begins = 0U, test_flash_writes = 0U, test_flash_aborts = 0U, test_boot_switches = 0U;
inline bool test_flash_begin_ok = true, test_flash_write_ok = true, test_flash_end_ok = true, test_flash_switch_ok = true;
inline bool test_flash_read_ok = true, test_flash_corrupt_on_read = false;
inline void (*test_ota_end_hook)() = nullptr;
inline const esp_partition_t *esp_ota_get_running_partition() { return &test_running_partition; }
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *) { return &test_next_partition; }
inline esp_err_t esp_ota_begin(const esp_partition_t *, uint32_t, esp_ota_handle_t *out) {
    test_flash_begins++; test_flash_bytes.clear(); *out = 1U; return test_flash_begin_ok ? ESP_OK : -1;
}
inline esp_err_t esp_ota_write(esp_ota_handle_t, const void *data, size_t size) {
    test_flash_writes++; if (!test_flash_write_ok) { return -1; }
    const auto *p = static_cast<const uint8_t *>(data);
    test_flash_bytes.insert(test_flash_bytes.end(), p, p + size); return ESP_OK;
}
inline esp_err_t esp_ota_end(esp_ota_handle_t) {
    if (test_ota_end_hook != nullptr) { auto hook = test_ota_end_hook; test_ota_end_hook = nullptr; hook(); }
    return test_flash_end_ok ? ESP_OK : -1;
}
inline esp_err_t esp_ota_set_boot_partition(const esp_partition_t *) {
    if (!test_flash_switch_ok) { return -1; } test_boot_switches++; return ESP_OK;
}
inline esp_err_t esp_ota_abort(esp_ota_handle_t) { test_flash_aborts++; return ESP_OK; }
inline esp_err_t esp_partition_read(const esp_partition_t *, size_t offset, void *out, size_t length) {
    if (!test_flash_read_ok || offset + length > test_flash_bytes.size()) { return -1; }
    auto *p = static_cast<uint8_t *>(out);
    for (size_t i = 0; i < length; i++) { p[i] = test_flash_bytes[offset + i]; }
    if (test_flash_corrupt_on_read && offset == 0U && length > 5U) { p[5] ^= 1U; }
    return ESP_OK;
}
#endif
