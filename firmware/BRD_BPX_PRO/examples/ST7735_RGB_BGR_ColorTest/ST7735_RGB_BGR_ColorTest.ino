/*
 * 檔案：BRD_BPX_PRO/examples/ST7735_RGB_BGR_ColorTest/ST7735_RGB_BGR_ColorTest.ino
 * [新增] ST7735S RGB/BGR 色序獨立診斷：每 3 秒切換一次 RGB/BGR。
 * [目的] 確認 0.96 吋 TFT 在不更換初始化 profile/座標的情況下，
 *        是否需要 ST7735 MADCTL_BGR 位元。
 * [硬體] ESP32-C3 SCK=GPIO21 / MOSI=GPIO20 / DC=GPIO2 / CS=GND / RESET=RC / BL=GPIO8
 * [注意] 此為獨立測試，會暫時取代 BRD 主韌體，測試完需重新燒錄主韌體。
 */
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

static Adafruit_ST7735 tft(&SPI, -1, 2, -1);
static bool is_bgr = false;
static uint32_t last_change = 0U;

static void apply_color_order(bool bgr) {
    // [新增] 維持 rotation=1 相同的方向位元；僅修改 0x08 色序位元。
    uint8_t madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV;
    if (bgr) {
        madctl |= ST7735_MADCTL_BGR;
    }
    tft.sendCommand(ST77XX_MADCTL, &madctl, 1U);
}

static void paint_test() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2, 3);
    tft.print(is_bgr ? "BGR MODE" : "RGB MODE");

    const uint16_t samples[] = {
        ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE,
        ST77XX_YELLOW, ST77XX_CYAN
    };
    const char *labels[] = {"R", "G", "B", "Y", "C"};
    for (int i = 0; i < 5; ++i) {
        const int x = 4 + i * 31;
        tft.fillRect(x, 22, 28, 27, samples[i]);
        tft.setTextColor(ST77XX_WHITE);
        tft.setCursor(x + 11, 53);
        tft.print(labels[i]);
    }
    tft.setCursor(2, 68);
    tft.print("R G B Y C from L to R");
}

void setup() {
    pinMode(8, OUTPUT);
    digitalWrite(8, LOW);
    delay(180);
    SPI.begin(21, -1, 20, -1);
    tft.initR(INITR_MINI160x80);
    tft.setSPISpeed(4000000UL);
    tft.setRotation(1);
    tft.invertDisplay(false);
    apply_color_order(is_bgr);
    paint_test();
    digitalWrite(8, HIGH);
    last_change = millis();
}

void loop() {
    if ((uint32_t)(millis() - last_change) >= 3000U) {
        last_change = millis();
        is_bgr = !is_bgr;
        apply_color_order(is_bgr);
        paint_test();
    }
}
