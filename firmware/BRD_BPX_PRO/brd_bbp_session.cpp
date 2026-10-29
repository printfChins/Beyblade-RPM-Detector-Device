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
    reset_capture();
    dirty_ = false;
}
void Session::reset_capture() {
    profiles_[0].reset();
    profiles_[1].reset();
    edge_max_[0] = edge_max_[1] = 0U;
    selected_edge_ = 0U;
    first_period_us_ = 0;
    launch_us_ = 0;
    representative_sp_ = 0;
    started_ = launched_ = pending_ = blocked_ = false;
}
void Session::period(uint32_t period_us, uint32_t event_us, uint8_t edge_level) {
    if (blocked_ || pending_ || period_us < 1000U || period_us > 1000000U || edge_level > 1U) {
        return;
    }
    /* [修改] 各極性最高值從所有有效同極性完整一圈週期取得。 */
    const uint16_t rpm = static_cast<uint16_t>(60000000UL / period_us);
    if (rpm > representative_sp_) { representative_sp_ = rpm; }
    if (rpm > edge_max_[edge_level]) { edge_max_[edge_level] = rpm; }
    if (!started_) {
        first_period_us_ = event_us;
        started_ = true;
    }
    /* [新增] 兩組曲線分開收集，傳送前才挑選包含整次最高 MAX 的一組。 */
    profiles_[edge_level].add_period_us(period_us);
}
void Session::set_representative_max(uint16_t exact_max) {
    if (!blocked_ && !pending_ && exact_max > representative_sp_) {
        representative_sp_ = exact_max;
    }
}
void Session::launched(uint32_t launch_us) {
    if (!blocked_ && !pending_ && !launched_) {
        /* [R2 修改] 發射只作標記；保留發射前的曲線及獨立代表值。 */
        launch_us_ = launch_us;
        launched_ = true;
    }
}
void Session::finish() {
    if (!blocked_ && launched_ && started_ && representative_sp_ != 0U) {
        /* [新增] 負緣與正緣 max 比較；相等優先採用取樣點較多組，仍相等取正緣。 */
        selected_edge_ = edge_max_[1] >= edge_max_[0] ? 1U : 0U;
        if (edge_max_[1] == edge_max_[0] && profiles_[0].size() > profiles_[1].size()) {
            selected_edge_ = 0U;
        }
        pending_ = true;
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
    if (!pending_ || static_cast<uint32_t>(now_us - first_period_us_) < minimum_delay_us) {
        return;
    }
    if (append(state_, profiles_[selected_edge_], representative_sp_)) {
        dirty_ = true;
    }
    abort();
}
uint8_t Session::flags(uint8_t percent, uint8_t warning, uint8_t critical) const {
    uint8_t value = started_ && !blocked_ ? 0x04U : 0U;
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
