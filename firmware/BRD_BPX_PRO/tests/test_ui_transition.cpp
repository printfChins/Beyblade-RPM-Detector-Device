/*
 * File: BRD_BPX_PRO/tests/test_ui_transition.cpp
 * [ADD] Pixel-accurate host regression for the four-page push-slide.
 */
#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "brd_ui_screen.h"
#include "brd_config.h"
#include "brd_ui_transition.h"
#include "brd_ui_style_scene.h"
#include "brd_ui_dirty_presenter.h"

namespace brd_ui {
namespace {
Adafruit_ST7735 panel(&SPI, -1, 2, -1);
}
Adafruit_ST7735& lcd() { return panel; }
}

int main() {
    using namespace brd_ui;
    assert(dirty_presenter::begin());
    Model m{};
    m.measurement.value = 2500U;
    m.measurement.loaded = true;
    m.battery = 84U;
    m.connected = true;
    m.ir1 = true;
    m.history_count = 4U;
    m.history[0] = 29980U;
    m.history[1] = 25000U;
    m.history[2] = 22000U;
    m.history[3] = 21000U;
    Curve curve{};
    curve.count = 10U;
    curve.peak = 29000U;
    for (unsigned i = 0; i < curve.count; ++i) {
        curve.rpm[i] = static_cast<uint16_t>(i * 2900U);
    }

    assert(transition::allowed(Page::Main, Page::Graph));
    assert(transition::allowed(Page::Graph, Page::History));
    assert(transition::allowed(Page::History, Page::IrTest));
    assert(transition::allowed(Page::IrTest, Page::Main));
    assert(!transition::allowed(Page::Main, Page::Max));
    assert(!transition::allowed(Page::LowBattery, Page::Main));
    assert(!transition::allowed(Page::Main, Page::Ota));

    const Page pages[] = {Page::Main, Page::Graph, Page::History,
                          Page::IrTest, Page::Main};
    for (uint8_t theme = 0; theme < 1U; ++theme) {
        GFXcanvas16 expected(160, 80);
        style_scene::draw(*dirty_presenter::canvas(), theme,
                          Page::Main, 0U, m, curve);
        dirty_presenter::invalidate();
        assert(dirty_presenter::flush(lcd()) == 25600UL);
        for (uint8_t p = 0; p < 4U; ++p) {
            assert(transition::start(pages[p], pages[p + 1], theme,
                                     m, curve, 2000U));
            expected.fillScreen(0U);
            style_scene::draw(expected, theme, pages[p + 1], 0U, m, curve);
            // First call advances, second at same timestamp is throttled.
            for (uint8_t step = 0; step < 5U; ++step) {
                uint32_t stamp = 2000U + static_cast<uint32_t>(step) * BRD_UI_TRANSITION_STEP_INTERVAL_MS;
                assert(transition::update(stamp));
                if (step < 4U) {
                    assert(transition::active());
                    assert(!transition::update(stamp));
                }
            }
            assert(!transition::active());
            assert(!transition::update(3000U));
            assert(std::memcmp(dirty_presenter::pixels(), expected.getBuffer(),
                               160U * 80U * sizeof(uint16_t)) == 0);
            assert(std::memcmp(lcd().pixels, expected.getBuffer(),
                               160U * 80U * sizeof(uint16_t)) == 0);
        }
    }
    // [ADD] Forced page override cancels the pending transition.
    assert(transition::start(Page::Main, Page::Graph, 0U, m, curve, 8000U));
    transition::cancel();
    assert(!transition::active());
    assert(!transition::update(9000U));
    std::puts("PASS: TORQUE only, four allowed routes, 5 nonblocking push-slide steps, pixel-perfect final image, immediate cancel.");
}
