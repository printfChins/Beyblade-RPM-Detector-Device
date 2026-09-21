/* 檔案位置: BRD_BBP/brd_bbp_session.h
   [BRD_BBP 新增] 完整新檔，命令與紀錄發布只由主 loop 呼叫。 */
#ifndef BRD_BBP_SESSION_H
#define BRD_BBP_SESSION_H
#include "brd_bbp_protocol.h"
namespace brd_bbp {
class Session {
public:
    Session();
    void restore(const State &state);
    void reset_capture();
    // [R3 修改] 所有有效讀值更新 MAX；record_profile 決定是否加入單圈曲線。
    void period(uint32_t period_us, uint32_t event_us, bool record_profile = true);
    // [R2 修改] 只標記發射事件，不清除已記錄的拉轉資料。
    void launched(uint32_t launch_us);
    void finish();
    void abort();
    void update(uint32_t now_us, uint32_t minimum_delay_us);
    uint8_t flags(uint8_t percent, uint8_t warning, uint8_t critical) const;
    uint8_t command(uint8_t command, uint8_t battery_raw, const uint8_t uid[6],
                    uint8_t status_flags, uint8_t out[PAGE_COUNT][PACKET_SIZE]);
    const State &state() const;
    bool dirty() const;
    void mark_saved();
private:
    State state_;
    Profile profile_;
    uint32_t first_period_us_;
    uint32_t launch_us_;
    uint16_t representative_sp_;
    bool started_, launched_, pending_, blocked_, dirty_;
};
}
#endif
