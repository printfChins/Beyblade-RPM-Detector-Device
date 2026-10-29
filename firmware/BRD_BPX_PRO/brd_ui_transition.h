/*
 * File: BRD_BPX_PRO/brd_ui_transition.h
 * [ADD] Four-page non-blocking horizontal push transition.
 * [KEEP] Official Adafruit ST7735/GFX and 8x16 Dirty Tile presenter.
 */
#ifndef BRD_UI_TRANSITION_H
#define BRD_UI_TRANSITION_H

#include <stdint.h>
#include "brd_ui_types.h"
#include "brd_ui_curve.h"

namespace brd_ui { namespace transition {

// [ADD] Only main -> graph -> history -> IR TEST -> main has a transition.
bool allowed(Page old_page, Page new_page);

// [ADD] Draw next page once into a staging canvas; false on allocation failure.
bool start(Page old_page, Page new_page, uint8_t theme,
           const Model &model, const Curve &curve, uint32_t now_ms);

// [ADD] Execute at most one animation step, without delay() or ISR work.
// Returns true only if an LCD update was transmitted.
bool update(uint32_t now_ms);

// [ADD] Warning/OTA/external page overrides immediately abort the transition.
void cancel();
bool active();

} }
#endif
