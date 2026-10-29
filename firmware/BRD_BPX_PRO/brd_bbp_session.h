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
    // [V1.3 修改] 所有正/負緣單圈讀值各自建檔，完成時選含最高 MAX 的極性。
    void period(uint32_t period_us, uint32_t event_us, uint8_t edge_level); /* [修改] 0=負緣，1=正緣 */
    void set_representative_max(uint16_t exact_max); /* [新增] 不受 BBP 8us 量化影響 */
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
    /* [V1.2 新增] 未發布的最後一筆不能在 OTA 暫停後才成為 dirty。 */
    bool pending() const { return pending_; }
    void mark_saved();
private:
    State state_;
    Profile profiles_[2]; /* [新增] 正、負緣各保存完整單圈的前 32 個週期。 */
    uint16_t edge_max_[2]; /* [新增] 各組所有週期的最高 RPM，不受 32 點數量限制。 */
    uint8_t selected_edge_;
    uint32_t first_period_us_;
    uint32_t launch_us_;
    uint16_t representative_sp_;
    bool started_, launched_, pending_, blocked_, dirty_;
};
}
#endif
