#ifndef TEST_PREFERENCES_H
#define TEST_PREFERENCES_H
#include <map>
#include <string>
#include <vector>
#include <cstring>
extern std::map<std::string, std::vector<uint8_t>> test_nvs;
extern unsigned test_flash_writes;
extern bool test_flash_fail;
inline void (*test_nvs_write_hook)() = nullptr;
class Preferences {
    std::string prefix_;
public:
    bool begin(const char *name, bool = false) { prefix_ = std::string(name) + "/"; return true; }
    bool isKey(const char *key) { return test_nvs.count(prefix_ + key) != 0U; }
    size_t getBytesLength(const char *key) { return test_nvs[prefix_ + key].size(); }
    size_t getBytes(const char *key, void *out, size_t size) {
        const auto &data = test_nvs[prefix_ + key];
        size_t length = data.size() < size ? data.size() : size;
        if (length != 0U) { std::memcpy(out, data.data(), length); }
        return length;
    }
    size_t putBytes(const char *key, const void *data, size_t size) {
        test_flash_writes++;
        if (test_nvs_write_hook) { auto hook=test_nvs_write_hook; test_nvs_write_hook=nullptr; hook(); }
        if (test_flash_fail) { return 0U; }
        const auto *bytes = static_cast<const uint8_t *>(data);
        test_nvs[prefix_ + key] = std::vector<uint8_t>(bytes, bytes + size);
        return size;
    }
    void end() {}
};
#endif
