/*
    檔案位置: BRD_BPX/brd_ota_protocol.cpp
    貼放位置: 與 BRD_BPX.ino 同一資料夾，完整新增檔案。
    [V1.2 新增] 分包位移、冪等重送、映像辨識及逾時；不依賴 Arduino。
*/
#include "brd_ota_protocol.h"
#include <string.h>
namespace brd_ota {
const char IMAGE_TAG[] = "BRD_BPX_OTA_IMAGE_V1_C3";
static uint32_t read32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8U) |
           (static_cast<uint32_t>(p[2]) << 16U) | (static_cast<uint32_t>(p[3]) << 24U);
}
static void write32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) { p[i] = static_cast<uint8_t>(value >> (8U * i)); }
}
Session::Session(Sink &sink) : sink_(sink) {
    /* [新增] KMP prefix table，標記跨越 MTU 分包仍能正確辨識。 */
    size_t matched = 0U;
    for (size_t i = 1U; IMAGE_TAG[i] != '\0'; i++) {
        while (matched > 0U && IMAGE_TAG[i] != IMAGE_TAG[matched]) { matched = tag_prefix_[matched - 1U]; }
        if (IMAGE_TAG[i] == IMAGE_TAG[matched]) { matched++; }
        tag_prefix_[i] = matched;
    }
}
Response Session::snapshot(uint8_t op, uint8_t id, Result result, const Policy &p) const {
    return {op, id, result, phase_, offset_, total_, sink_.capacity(), p.max_data,
            static_cast<uint8_t>((p.charging ? 1U : 0U) | (p.power_ok ? 2U : 0U) |
                                 (p.may_start ? 4U : 0U) | FEATURE_BATCH_ACK)};
}
void Session::fail() {
    sink_.abort(); phase_ = ERROR; hash_ready_ = false; batch_mode_ = false;
}
bool Session::check_image(const uint8_t *d, size_t n) {
    if (prefix_size_ < sizeof(prefix_)) {
        size_t count = sizeof(prefix_) - prefix_size_; if (count > n) { count = n; }
        memcpy(prefix_ + prefix_size_, d, count); prefix_size_ += count;
    }
    if (prefix_size_ > 0U && prefix_[0] != 0xE9U) { return false; }
    if (prefix_size_ >= 14U && (prefix_[12] != 5U || prefix_[13] != 0U)) { return false; }
    if (prefix_size_ == sizeof(prefix_) && read32(prefix_ + 32U) != 0xABCD5432UL) { return false; }
    for (size_t i = 0; !tag_found_ && i < n; i++) {
        while (tag_match_ > 0U && d[i] != static_cast<uint8_t>(IMAGE_TAG[tag_match_])) {
            tag_match_ = tag_prefix_[tag_match_ - 1U];
        }
        if (d[i] == static_cast<uint8_t>(IMAGE_TAG[tag_match_])) { tag_match_++; }
        if (IMAGE_TAG[tag_match_] == '\0') { tag_found_ = true; }
    }
    return true;
}
Response Session::handle(const uint8_t *d, size_t n, uint32_t now, const Policy &p) {
    uint8_t op = d != nullptr && n > 0U ? d[0] : 0U;
    uint8_t id = d != nullptr && n > 1U ? d[1] : 0U;
    auto reply = [&](Result result) { return snapshot(op, id, result, p); };
    if (d == nullptr || n < 2U) { return reply(BAD_LENGTH); }
    if (op == INFO) { return reply(n == 2U ? OK : BAD_LENGTH); }
    if (id == 0U) { return reply(SESSION); }
    if (op == HASH) {
        if (n != 18U) { return reply(BAD_LENGTH); }
        if (busy()) {
            return reply(phase_ == RECEIVING && id == id_ && memcmp(hash_, d + 2U, 16U) == 0 ? OK : BUSY);
        }
        id_ = id; memcpy(hash_, d + 2U, 16U); hash_ready_ = true;
        phase_ = IDLE; offset_ = total_ = 0U; batch_mode_ = false;
        return reply(OK);
    }
    if (op == BEGIN) {
        if (n != 6U) { return reply(BAD_LENGTH); }
        uint32_t size = read32(d + 2U);
        if (busy()) {
            return reply(phase_ == RECEIVING && id == id_ && size == total_ ? OK : BUSY);
        }
        if (!hash_ready_ || id != id_) { return reply(SESSION); }
        if (!p.power_ok) { return reply(POWER); }
        if (!p.may_start) { return reply(BUSY); }
        if (size < sizeof(prefix_) || size > sink_.capacity()) { return reply(SIZE); }
        if (!sink_.begin(size, hash_)) { fail(); return reply(WRITE); }
        phase_ = RECEIVING; total_ = size; offset_ = 0U; last_ms_ = now; batch_mode_ = false;
        prefix_size_ = tag_match_ = last_length_ = 0U; tag_found_ = false;
        return reply(OK);
    }
    /* [V1.2 新增] SYNC 不修改 offset，只開啟 DATA 批次模式並回報累計位移。
       控制命令必須排在先前 DATA 後面處理，才能當作批次寫入屏障。 */
    if (op == SYNC) {
        if (n != 2U) { return reply(BAD_LENGTH); }
        if (id != id_ || phase_ != RECEIVING) { return reply(SESSION); }
        batch_mode_ = true; last_ms_ = now;
        return reply(OK);
    }
    if (op != DATA && op != END && op != ABORT) { return reply(BAD_COMMAND); }
    if ((op == END || op == ABORT) && n != 2U) { return reply(BAD_LENGTH); }
    if (id != id_) { return reply(SESSION); }
    if (op == ABORT) {
        /* [新增] 提交後不可假裝取消成功；開機分區已更新，等待重啟。 */
        if (phase_ == COMPLETE) { return reply(BUSY); }
        if (phase_ == RECEIVING) { sink_.abort(); }
        phase_ = IDLE; hash_ready_ = false; batch_mode_ = false; offset_ = total_ = 0U;
        return reply(OK);
    }
    if (op == END && phase_ == COMPLETE) { return reply(OK); }
    if (phase_ != RECEIVING) { return reply(SESSION); }
    if (op == END) {
        if (offset_ != total_) { return reply(SIZE); }
        if (!tag_found_ || prefix_size_ != sizeof(prefix_)) { fail(); return reply(IMAGE); }
        if (!sink_.finish()) { fail(); return reply(VERIFY); }
        phase_ = COMPLETE; hash_ready_ = false;
        return reply(OK);
    }
    if (n <= DATA_HEADER || n > DATA_HEADER + MAX_DATA || n - DATA_HEADER > p.max_data) {
        return reply(BAD_LENGTH);
    }
    uint32_t offset = read32(d + 2U); size_t length = n - DATA_HEADER;
    if (last_length_ != 0U && offset == last_offset_ && length == last_length_ &&
        memcmp(d + DATA_HEADER, last_data_, length) == 0) {
        last_ms_ = now; return reply(OK);
    }
    if (offset != offset_ || length > total_ - offset_) { return reply(OFFSET); }
    if (!check_image(d + DATA_HEADER, length)) { fail(); return reply(IMAGE); }
    if (!sink_.write(d + DATA_HEADER, length)) { fail(); return reply(WRITE); }
    memcpy(last_data_, d + DATA_HEADER, length); last_offset_ = offset; last_length_ = length;
    offset_ += static_cast<uint32_t>(length); last_ms_ = now;
    return reply(OK);
}
bool Session::tick(uint32_t now) {
    if (phase_ != RECEIVING || static_cast<uint32_t>(now - last_ms_) < TIMEOUT_MS) { return false; }
    fail(); return true;
}
void Session::disconnect() {
    if (phase_ == COMPLETE) { return; }
    if (phase_ == RECEIVING) { sink_.abort(); }
    phase_ = IDLE; hash_ready_ = false; batch_mode_ = false; offset_ = total_ = 0U;
}
void encode(const Response &r, uint8_t out[RESPONSE_SIZE]) {
    out[0] = r.opcode; out[1] = r.id; out[2] = r.result; out[3] = r.phase;
    write32(out + 4U, r.offset); write32(out + 8U, r.total); write32(out + 12U, r.capacity);
    out[16] = static_cast<uint8_t>(r.max_data); out[17] = static_cast<uint8_t>(r.max_data >> 8U);
    out[18] = r.flags; out[19] = 1U;
}
}
