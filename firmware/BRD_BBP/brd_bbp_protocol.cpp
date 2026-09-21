/* 檔案位置: BRD_BBP/brd_bbp_protocol.cpp
   [BRD_BBP 新增] 完整新檔，放在主程式同一資料夾。 */
// [BRD_BBP 新增] 實作固定大小緩衝、環形紀錄、封包編碼及嚴格解碼。
#include "brd_bbp_protocol.h"

#include <string.h>

namespace brd_bbp {

static void put16(uint8_t *out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value);
    out[1] = static_cast<uint8_t>(value >> 8);
}

static uint16_t get16(const uint8_t *in) {
    return static_cast<uint16_t>(in[0] | (static_cast<uint16_t>(in[1]) << 8));
}

static uint32_t get32(const uint8_t *in) {
    return static_cast<uint32_t>(in[0]) |
           (static_cast<uint32_t>(in[1]) << 8) |
           (static_cast<uint32_t>(in[2]) << 16) |
           (static_cast<uint32_t>(in[3]) << 24);
}

static void put32(uint8_t *out, uint32_t value) {
    for (uint8_t i = 0; i < 4; ++i)
        out[i] = static_cast<uint8_t>(value >> (8 * i));
}

static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffffu;
}

Profile::Profile() {
    reset();
}

void Profile::reset() {
    memset(values_, 0, sizeof values_);
    minimum_ = 0;
    size_ = 0;
}

void Profile::add_period_us(uint32_t period_us) {
    /* [API V1.1 修改] 只保留前 32 個有效週期。
       [刪減] 後續更高峰重新建立曲線視窗及 recent_ 緩衝。 */
    if (period_us < 1000U || size_ >= CURVE_SIZE) {
        return;
    }
    uint32_t rounded = period_us / 8U + (period_us % 8U >= 4U ? 1U : 0U);
    if (rounded > 65535U) {
        rounded = 65535U;
    }
    const uint16_t raw = static_cast<uint16_t>(rounded);
    values_[size_++] = raw;
    if (minimum_ == 0U || raw < minimum_) {
        minimum_ = raw;
    }
}

uint16_t Profile::peak_rpm() const {
    return minimum_ == 0 ? 0 : static_cast<uint16_t>(7500000u / minimum_);
}

const uint16_t *Profile::values() const {
    return values_;
}

uint8_t Profile::size() const {
    return size_;
}

void clear(State &state) {
    memset(&state, 0, sizeof state);
}

bool append(State &state, const Profile &profile, uint16_t representative_sp) {
    /* [API V1.1 修改] History 使用整次量測代表值；沒有發射後週期時仍保留 Shot。 */
    if (representative_sp == 0U || representative_sp > 60000U) {
        return false;
    }
    state.records[state.next] = representative_sp;
    state.next = static_cast<uint8_t>((state.next + 1) % HISTORY_SIZE);
    if (state.count < HISTORY_SIZE)
        ++state.count;
    ++state.total;
    if (representative_sp > state.lifetime_max)
        state.lifetime_max = representative_sp;
    memset(state.curve, 0, sizeof state.curve);
    memcpy(state.curve, profile.values(), profile.size() * sizeof state.curve[0]);
    return true;
}

void build_header(const State &state, uint8_t flags, uint8_t battery_raw,
                  const uint8_t uid[6], uint8_t out[PACKET_SIZE]) {
    memset(out, 0, PACKET_SIZE);
    out[0] = 0xa0;
    out[1] = 0x3a;
    out[3] = flags;
    out[4] = battery_raw > 250 ? 250 : battery_raw;
    put16(out + 7, state.lifetime_max);
    put16(out + 9, static_cast<uint16_t>(state.total));
    memcpy(out + 11, uid, 6);
}

void build_dump(const State &state, uint8_t out[PAGE_COUNT][PACKET_SIZE]) {
    memset(out, 0, PAGE_COUNT * PACKET_SIZE);
    for (uint8_t i = 0; i < 8; ++i)
        out[i][0] = static_cast<uint8_t>(0xb0 + i);
    for (uint8_t i = 0; i < 4; ++i)
        out[8 + i][0] = static_cast<uint8_t>(0x70 + i);

    const uint8_t oldest = state.count == HISTORY_SIZE ? state.next : 0;
    for (uint8_t i = 0; i < state.count; ++i) {
        const uint8_t record = static_cast<uint8_t>((oldest + i) % HISTORY_SIZE);
        put16(out[i / 8] + 1 + (i % 8) * 2, state.records[record]);
    }
    out[6][5] = 0;
    out[6][6] = 0;
    put16(out[6] + 7, state.lifetime_max);
    put16(out[6] + 9, static_cast<uint16_t>(state.total));
    out[6][11] = state.count;
    uint8_t checksum = 0;
    for (uint8_t page = 0; page < 7; ++page)
        for (uint8_t byte = 1; byte < PACKET_SIZE; ++byte)
            checksum = static_cast<uint8_t>(checksum + out[page][byte]);
    out[7][16] = checksum;
    for (uint8_t i = 0; i < CURVE_SIZE; ++i)
        put16(out[8 + i / 8] + 1 + (i % 8) * 2, state.curve[i]);
}

void encode_storage(const State &state, uint8_t out[STORAGE_SIZE]) {
    memset(out, 0, STORAGE_SIZE);
    memcpy(out, "BBPS", 4);
    out[4] = 1;
    put32(out + 5, state.total);
    put16(out + 9, state.lifetime_max);
    out[11] = state.count;
    out[12] = state.next;
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i)
        put16(out + 13 + i * 2, state.records[i]);
    for (uint8_t i = 0; i < CURVE_SIZE; ++i)
        put16(out + 113 + i * 2, state.curve[i]);
    put32(out + 177, calculate_crc32(out, 177));
}

bool decode_storage(const uint8_t *data, size_t length, State &out) {
    if (data == 0 || length != STORAGE_SIZE || memcmp(data, "BBPS", 4) != 0 || data[4] != 1)
        return false;
    if (get32(data + 177) != calculate_crc32(data, 177))
        return false;

    State candidate;
    clear(candidate);
    candidate.total = get32(data + 5);
    candidate.lifetime_max = get16(data + 9);
    candidate.count = data[11];
    candidate.next = data[12];
    if (candidate.count > HISTORY_SIZE || candidate.next >= HISTORY_SIZE)
        return false;
    if (candidate.count < HISTORY_SIZE && candidate.next != candidate.count)
        return false;
    uint16_t record_max = 0;
    for (uint8_t i = 0; i < HISTORY_SIZE; ++i) {
        candidate.records[i] = get16(data + 13 + i * 2);
        const bool valid = candidate.count == HISTORY_SIZE || i < candidate.count;
        if (valid && (candidate.records[i] == 0 || candidate.records[i] > 60000))
            return false;
        if (!valid && candidate.records[i] != 0)
            return false;
        if (candidate.records[i] > record_max)
            record_max = candidate.records[i];
    }
    if (candidate.lifetime_max > 60000 || candidate.lifetime_max < record_max)
        return false;

    bool curve_ended = false;
    uint16_t minimum = 0;
    for (uint8_t i = 0; i < CURVE_SIZE; ++i) {
        candidate.curve[i] = get16(data + 113 + i * 2);
        if (candidate.curve[i] == 0) {
            curve_ended = true;
        } else {
            if (curve_ended || candidate.curve[i] < 125)
                return false;
            if (minimum == 0 || candidate.curve[i] < minimum)
                minimum = candidate.curve[i];
        }
    }
    if (candidate.count == 0) {
        if (candidate.lifetime_max != 0 || record_max != 0 || minimum != 0)
            return false;
    }
    /* [API V1.1 刪減] 不再要求最後一筆 History 等於 Profile 峰值。
       [新增] 有效 Shot 可有全零曲線；CRC、範圍、ring 與零尾端檢查仍保留。 */
    out = candidate;
    return true;
}

} // namespace brd_bbp
