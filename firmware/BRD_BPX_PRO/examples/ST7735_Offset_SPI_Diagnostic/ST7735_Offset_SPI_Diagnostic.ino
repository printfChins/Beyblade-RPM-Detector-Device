/*
 * BRD BPX PRO / ST7735 0.96 inch 160x80 diagnostics.
 * [NEW] Direct draw versus partial RGB565 bitmap transfer.
 * [IMPORTANT] Standalone sketch. Flash it only for LCD diagnosis.
 * [IMPORTANT] LCD CS=GND, RST external 10k/1uF RC.
 * [IMPORTANT] Uses Adafruit ST7735 + Adafruit GFX, no custom LCD driver.
 * [CONFIG] TEST_PANEL_PROFILE: 0=MINI, 1=MINI_PLUGIN.
 * [CONFIG] TEST_SPI_HZ: Keep 4000000 for the first measurement.
 * [CONFIG] TEST_INVERSION follows the selected Adafruit init profile.
 */
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TEST_PANEL_PROFILE 0
#define TEST_SPI_HZ 4000000UL
#define TEST_DC_PIN 2
#define TEST_SCK_PIN 21
#define TEST_MOSI_PIN 20
#define TEST_BL_PIN 8

#if TEST_PANEL_PROFILE == 0
#define TEST_INIT_PROFILE INITR_MINI160x80
#define TEST_INVERSION false
#else
#define TEST_INIT_PROFILE INITR_MINI160x80_PLUGIN
#define TEST_INVERSION true
#endif

Adafruit_ST7735 screen(&SPI, -1, TEST_DC_PIN, -1);
static uint16_t tile[8 * 16];
static uint8_t phase = 0;
static uint32_t last_switch = 0;

static void print_label(int16_t x, int16_t y, const char *value, uint16_t color) {
    screen.setTextSize(1);
    screen.setTextColor(color);
    screen.setCursor(x, y);
    screen.print(value);
}

static void draw_reference_direct() {
    // [TEST A] All geometry goes through Adafruit draw primitives.
    screen.fillScreen(ST77XX_BLACK);
    screen.drawRect(0, 0, 160, 80, ST77XX_WHITE);
    screen.drawFastHLine(0, 1, 160, ST77XX_CYAN);
    screen.drawFastVLine(1, 0, 80, ST77XX_MAGENTA);
    screen.fillRect(3, 3, 15, 12, ST77XX_RED);
    screen.fillRect(142, 3, 15, 12, ST77XX_GREEN);
    screen.fillRect(3, 65, 15, 12, ST77XX_BLUE);
    screen.fillRect(142, 65, 15, 12, ST77XX_YELLOW);
    print_label(25, 19, "DIRECT GFX", ST77XX_WHITE);
    print_label(25, 31, "160x80 BORDER", ST77XX_CYAN);
    print_label(25, 43, "L/T EDGE TEST", ST77XX_WHITE);
}

static void draw_reference_tiles() {
    // [TEST B] Full refresh, 8x16 tiles; official drawRGBBitmap path.
    // Test patterns include boundaries and alternating RGB565 values.
    screen.fillScreen(ST77XX_BLACK);
    for (int y = 0; y < 80; y += 16) {
        for (int x = 0; x < 160; x += 8) {
            uint16_t color;
            switch (((x / 8) + (y / 16)) & 3) {
            case 0: color = ST77XX_RED; break;
            case 1: color = ST77XX_GREEN; break;
            case 2: color = ST77XX_BLUE; break;
            default: color = ST77XX_CYAN; break;
            }
            for (int i = 0; i < 8 * 16; ++i) {
                tile[i] = color;
            }
            screen.drawRGBBitmap(x, y, tile, 8, 16);
        }
    }
    // Overlay markers drawn directly over the transferred tiles.
    screen.drawRect(0, 0, 160, 80, ST77XX_WHITE);
    print_label(30, 35, "TILE BITMAP", ST77XX_WHITE);
}

static void draw_reference_lines() {
    // [TEST C] A full 1-pixel line-grid catches row/column address drift.
    screen.fillScreen(ST77XX_BLACK);
    for (int x = 0; x < 160; x += 10) {
        screen.drawFastVLine(x, 0, 80, ST77XX_CYAN);
    }
    for (int y = 0; y < 80; y += 10) {
        screen.drawFastHLine(0, y, 160, ST77XX_MAGENTA);
    }
    screen.drawRect(0, 0, 160, 80, ST77XX_WHITE);
    print_label(28, 34, "GRID 10x10", ST77XX_WHITE);
}

void setup() {
    digitalWrite(TEST_BL_PIN, LOW);
    pinMode(TEST_BL_PIN, OUTPUT);
    delay(220);
    SPI.begin(TEST_SCK_PIN, -1, TEST_MOSI_PIN, -1);
    screen.initR(TEST_INIT_PROFILE);
    delay(100);
    screen.initR(TEST_INIT_PROFILE);
    screen.setSPISpeed(TEST_SPI_HZ);
    screen.setRotation(1);
    screen.invertDisplay(TEST_INVERSION);
    screen.setTextWrap(false);
    draw_reference_direct();
    digitalWrite(TEST_BL_PIN, HIGH);
    last_switch = millis();
}

void loop() {
    if ((uint32_t)(millis() - last_switch) < 3000UL) {
        return;
    }
    last_switch = millis();
    phase = (uint8_t)((phase + 1U) % 3U);
    switch (phase) {
    case 0: draw_reference_direct(); break;
    case 1: draw_reference_tiles(); break;
    default: draw_reference_lines(); break;
    }
}
