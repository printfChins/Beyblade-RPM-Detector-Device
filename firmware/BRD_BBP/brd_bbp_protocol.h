/* 檔案位置: BRD_BBP/brd_bbp_protocol.h
   [BRD_BBP 新增] 完整新檔，放在主程式同一資料夾。 */
// [BRD_BBP 新增] 戰鬥通行證封包、曲線與持久化資料的零配置介面。
#ifndef BRD_BBP_PROTOCOL_H
#define BRD_BBP_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

namespace brd_bbp {

constexpr size_t PACKET_SIZE = 17;
constexpr size_t PAGE_COUNT = 12;
constexpr size_t HISTORY_SIZE = 50;
constexpr size_t CURVE_SIZE = 32;
constexpr size_t STORAGE_SIZE = 181;

struct State {
    uint32_t total;
    uint16_t lifetime_max;
    uint8_t count;
    uint8_t next;
    uint16_t records[HISTORY_SIZE];
    uint16_t curve[CURVE_SIZE];
};

class Profile {
public:
    Profile();
    void reset();
    void add_period_us(uint32_t period_us);
    uint16_t peak_rpm() const;
    const uint16_t *values() const;
    uint8_t size() const;

private:
    uint16_t values_[CURVE_SIZE];
    uint16_t minimum_;
    uint8_t size_;
};

void clear(State &state);
// [API V1.1 修改] 代表 SP 與發射後 Profile 分開保存，允許空曲線。
bool append(State &state, const Profile &profile, uint16_t representative_sp);
void build_header(const State &state, uint8_t flags, uint8_t battery_raw,
                  const uint8_t uid[6], uint8_t out[PACKET_SIZE]);
void build_dump(const State &state, uint8_t out[PAGE_COUNT][PACKET_SIZE]);
void encode_storage(const State &state, uint8_t out[STORAGE_SIZE]);
bool decode_storage(const uint8_t *data, size_t length, State &out);

} // namespace brd_bbp

#endif
