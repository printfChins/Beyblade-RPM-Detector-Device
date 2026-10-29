/* File: BRD_BPX_PRO/brd_ui_motion.h
 * [修改] 固定 TORQUE 單一 160x80 風格，保留非阻塞 dirty 更新 API。
 */
#ifndef BRD_UI_MOTION_H
#define BRD_UI_MOTION_H
#include <stdint.h>
#include "brd_ui_types.h"
#include "brd_ui_curve.h"
namespace brd_ui { namespace motion {
constexpr uint8_t THEME_COUNT=1U; // [修改] 風格三 TORQUE 唯一主題。
void begin(uint8_t theme_index);
bool ready();
void set_theme(uint8_t theme_index);
uint8_t theme();
bool handles(Page page);
void enter(Page page,uint32_t now_ms);
// [ADD] Adopt incoming scene already present in Dirty Tile framebuffer.
void adopt_rendered(Page page,uint32_t now_ms);
bool update(Page page,const Model &model,const Curve &curve,uint32_t now_ms);
} }
#endif
