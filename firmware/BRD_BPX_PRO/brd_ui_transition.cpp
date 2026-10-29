/*
 * File: BRD_BPX_PRO/brd_ui_transition.cpp
 * [ADD] Efficient four-page push-slide transition, 160x80 RGB565.
 * [ADD] Stage incoming scene only once. Shift the existing presenter canvas
 *       in-place and append an incoming strip at each animation tick.
 * [KEEP] No ST7735 register/SPI driver code. The existing Dirty Tile
 *        presenter sends changed pixels through Adafruit GFX.
 * [KEEP] Does not allocate a second copy of the outgoing frame.
 * [NOTE] 5 x 32-pixel steps. Horizontal scrolling changes most pixels,
 *        so each step can transfer up to 25,600 bytes at the configured SPI
 *        clock. It is intentionally restricted to manual four-page navigation.
 */
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <new>
#include <cstring>
#include "brd_ui_transition.h"
#include "brd_config.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_dirty_presenter.h"
#include "brd_ui_screen.h"

namespace brd_ui { namespace transition {
namespace {
constexpr uint16_t WIDTH = 160U;
constexpr uint16_t HEIGHT = 80U;
constexpr uint16_t STEP_PIXELS = 32U;
constexpr uint16_t STEP_COUNT = WIDTH / STEP_PIXELS;
// [修改] 切頁排程改由 brd_config.h 決定；40 ms/步，共 5 步。
constexpr uint32_t STEP_INTERVAL_MS = BRD_UI_TRANSITION_STEP_INTERVAL_MS;

GFXcanvas16 *incoming = nullptr;
bool running = false;
uint8_t completed_steps = 0U;
uint32_t last_step_ms = 0U;

// [ADD] The current framebuffer is the outgoing scene at start().
// At each tick its rows are shifted left, then the rightmost stripe of the
// incoming scene is copied in. This yields a true two-page push transition.
void advance_one_step() {
    uint16_t *outgoing = dirty_presenter::pixels();
    const uint16_t *next_pixels = incoming ?
        static_cast<const uint16_t*>(incoming->getBuffer()) : nullptr;
    if (!outgoing || !next_pixels) {
        running = false;
        return;
    }
    const uint16_t source_x = static_cast<uint16_t>(completed_steps) * STEP_PIXELS;
    for (uint16_t y = 0; y < HEIGHT; ++y) {
        uint16_t *row = outgoing + static_cast<uint32_t>(y) * WIDTH;
        const uint16_t *next_row = next_pixels + static_cast<uint32_t>(y) * WIDTH;
        std::memmove(row, row + STEP_PIXELS,
                     (WIDTH - STEP_PIXELS) * sizeof(uint16_t));
        std::memcpy(row + WIDTH - STEP_PIXELS, next_row + source_x,
                    STEP_PIXELS * sizeof(uint16_t));
    }
    ++completed_steps;
}
} // namespace

bool allowed(Page old_page, Page new_page) {
    return (old_page == Page::Main && new_page == Page::Graph) ||
           (old_page == Page::Graph && new_page == Page::History) ||
           (old_page == Page::History && new_page == Page::IrTest) ||
           (old_page == Page::IrTest && new_page == Page::Main);
}

bool start(Page old_page, Page new_page, uint8_t theme,
           const Model &model, const Curve &curve, uint32_t now_ms) {
    if (running || !allowed(old_page, new_page) ||
        !dirty_presenter::pixels()) {
        return false;
    }
    // [ADD] 25.6 KB extra only; allocated once at first page transition.
    if (!incoming) {
        incoming = new(std::nothrow) GFXcanvas16(WIDTH, HEIGHT);
    }
    if (!incoming || !incoming->getBuffer()) {
        return false; // [SAFE] Caller immediately switches page, no animation.
    }
    incoming->setTextWrap(false);
    style_scene::draw(*incoming, theme, new_page, 0U, model, curve);
    completed_steps = 0U;
    last_step_ms = now_ms - STEP_INTERVAL_MS;
    running = true;
    return true;
}

bool update(uint32_t now_ms) {
    if (!running ||
        static_cast<uint32_t>(now_ms - last_step_ms) < STEP_INTERVAL_MS) {
        return false;
    }
    last_step_ms = now_ms;
    advance_one_step();
    const bool sent = dirty_presenter::flush(lcd()) != 0U;
    if (completed_steps >= STEP_COUNT) {
        running = false;
    }
    return sent;
}

void cancel() {
    // [ADD] Core immediately renders a full replacement on an urgent page.
    running = false;
    completed_steps = 0U;
}

bool active() { return running; }

} }
