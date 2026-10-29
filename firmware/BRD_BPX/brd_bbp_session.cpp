/* 檔案位置: BRD_BBP/brd_bbp_session.cpp
   [BRD_BBP 新增] 先凍結完整曲線，再一起發布紀錄、次數與最高值。 */
#include "brd_bbp_session.h"
namespace brd_bbp {
Session::Session() : first_period_us_(0), launch_us_(0), representative_sp_(0),
                     started_(false), launched_(false), pending_(false),
                     blocked_(false), dirty_(false) {
    clear(state_);
}
void Session::restore(const State &state) {
    state_ = state;
    completed_head_ = completed_count_ = 0U; pending_ = false;
    reset_capture();
    dirty_ = false;
}
void Session::reset_capture() {
    profile_.reset();
    first_period_us_ = 0;
    launch_us_ = 0;
    representative_sp_ = 0;
    /* [刪減] 新採樣不可清除已完成且等待發布的記錄。 */
    started_ = launched_ = blocked_ = false;
}
void Session::period(uint32_t period_us, uint32_t event_us, bool record_profile) {
    if (blocked_ || period_us < 1000U || period_us > 1000000U) {
        return;
    }
    /* [API V1.1 新增] 整次量測 MAX 作為 BRD 的代表 SP 測試映射。
       Profile 未開始或已滿 32 點時，仍繼續更新代表值。 */
    const uint16_t rpm = static_cast<uint16_t>(60000000U / period_us);
    if (rpm > representative_sp_) {
        representative_sp_ = rpm;
    }
    if (!started_) {
        first_period_us_ = event_us;
        started_ = true;
    }
    /* [R3 修改] 曲線只保存本次參考邊沿的整圈週期，一圈一筆。
       [保留] 裝載後取得有效參考週期即起錄，發射不清空前段資料。 */
    if (record_profile) {
        profile_.add_period_us(period_us);
    }
}
void Session::launched(uint32_t launch_us) {
    if (!blocked_ && !launched_) {
        /* [R2 修改] 發射只作標記；保留發射前的曲線及獨立代表值。 */
        launch_us_ = launch_us;
        launched_ = true;
    }
}
void Session::finish() {
    if (!blocked_ && launched_ && started_ && representative_sp_ != 0U) {
        /* [新增] 容量滿時先發布最舊完成資料，採樣不等待 BLE。 */
        if (completed_count_ == COMPLETED_SIZE) {
            const Completed &oldest = completed_[completed_head_];
            if (append(state_, oldest.profile, oldest.sp)) { dirty_ = true; }
            completed_head_ = static_cast<uint8_t>((completed_head_ + 1U) % COMPLETED_SIZE);
            completed_count_--;
        }
        uint8_t slot = static_cast<uint8_t>((completed_head_ + completed_count_) % COMPLETED_SIZE);
        completed_[slot] = {profile_, first_period_us_, representative_sp_};
        completed_count_++;
        pending_ = true;
        blocked_ = true;
    } else {
        abort();
    }
}
void Session::abort() {
    reset_capture();
    /* [新增] 清除、溢位或低電之後，不把同一發剩餘脈衝另算一筆。 */
    blocked_ = true;
}
void Session::update(uint32_t now_us, uint32_t minimum_delay_us) {
    while (completed_count_ != 0U) {
        const Completed &shot = completed_[completed_head_];
        if (static_cast<uint32_t>(now_us - shot.first_us) < minimum_delay_us) { break; }
        if (append(state_, shot.profile, shot.sp)) { dirty_ = true; }
        completed_head_ = static_cast<uint8_t>((completed_head_ + 1U) % COMPLETED_SIZE);
        completed_count_--;
    }
    pending_ = completed_count_ != 0U;
    /* [刪減] 發布舊資料不再 abort 正在進行的新採樣。 */
}
uint8_t Session::flags(uint8_t percent, uint8_t warning, uint8_t critical) const {
    /* [修正] 嚴格 BBP 模式在完成資料實際發布前仍保留 bit2。 */
    uint8_t value = ((started_ && !blocked_) || pending_) ? 0x04U : 0U;
    if (percent <= warning) {
        value |= 0x10U;
    }
    if (percent <= critical) {
        value |= 0x01U;
    }
    return value;
}
uint8_t Session::command(uint8_t command, uint8_t battery_raw, const uint8_t uid[6],
                         uint8_t status_flags, uint8_t out[PAGE_COUNT][PACKET_SIZE]) {
    switch (command) {
        case 0x51:
            build_header(state_, status_flags, battery_raw, uid, out[0]);
            return 1U;
        case 0x74:
            build_dump(state_, out);
            return static_cast<uint8_t>(PAGE_COUNT);
        case 0x75:
            clear(state_);
            completed_head_ = completed_count_ = 0U; pending_ = false;
            abort();
            dirty_ = true;
            return 0U;
        case 0x61:
        default:
            /* [新增] 未確認命令不回傳自創 ACK，不修改資料。 */
            return 0U;
    }
}
const State &Session::state() const {
    return state_;
}
bool Session::dirty() const {
    return dirty_;
}
void Session::mark_saved() {
    dirty_ = false;
}
}
