/*
    檔案位置: BRD_BPX/brd_ota_protocol.h
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [V1.2 新增] 與硬體無關的 BLE OTA 協議、狀態及 Flash 邊界。
*/
#ifndef BRD_OTA_PROTOCOL_H
#define BRD_OTA_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

namespace brd_ota {
constexpr size_t RESPONSE_SIZE = 20U;
constexpr size_t MAX_DATA = 240U;
constexpr size_t DATA_HEADER = 6U;
constexpr uint32_t TIMEOUT_MS = 15000UL;
extern const char IMAGE_TAG[];
/* [V1.2 新增] SYNC 查詢累計接收位置並啟用批次資料傳送；舊主機完全相容。 */
enum Opcode : uint8_t { INFO = 1, HASH = 2, BEGIN = 3, END = 4, ABORT = 5, SYNC = 6, DATA = 16 };
constexpr uint8_t FEATURE_BATCH_ACK = 0x08U;
enum Phase : uint8_t { IDLE = 0, RECEIVING = 1, COMPLETE = 2, ERROR = 3 };
enum Result : uint8_t {
    OK = 0, BAD_LENGTH, BUSY, POWER, SIZE, SESSION, OFFSET, IMAGE, WRITE,
    VERIFY, TIMEOUT, BAD_COMMAND
};
struct Policy {
    bool may_start;
    bool power_ok;
    bool charging;
    uint16_t max_data;
};
struct Response {
    uint8_t opcode, id;
    Result result;
    Phase phase;
    uint32_t offset, total, capacity;
    uint16_t max_data;
    uint8_t flags;
};
void encode(const Response &response, uint8_t out[RESPONSE_SIZE]);
class Sink {
public:
    virtual ~Sink() = default;
    virtual uint32_t capacity() const = 0;
    virtual bool begin(uint32_t size, const uint8_t md5[16]) = 0;
    virtual bool write(const uint8_t *data, size_t length) = 0;
    virtual bool finish() = 0;
    virtual void abort() = 0;
};
class Session {
public:
    explicit Session(Sink &sink);
    Response handle(const uint8_t *data, size_t length, uint32_t now, const Policy &policy);
    bool tick(uint32_t now);
    void disconnect();
    bool busy() const { return phase_ == RECEIVING || phase_ == COMPLETE; }
    Phase phase() const { return phase_; }
    bool batch_mode() const { return batch_mode_; }
    Response snapshot(uint8_t opcode, uint8_t id, Result result, const Policy &policy) const;
private:
    Sink &sink_;
    Phase phase_ = IDLE;
    uint8_t id_ = 0U;
    bool hash_ready_ = false, tag_found_ = false, batch_mode_ = false;
    uint8_t hash_[16] = {}, prefix_[36] = {}, last_data_[MAX_DATA] = {};
    size_t prefix_size_ = 0U, tag_match_ = 0U, last_length_ = 0U, tag_prefix_[32] = {};
    uint32_t offset_ = 0U, total_ = 0U, last_ms_ = 0U, last_offset_ = 0U;
    void fail();
    bool check_image(const uint8_t *data, size_t length);
};
}
#endif
