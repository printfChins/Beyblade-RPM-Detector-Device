/*
 * 檔案: BRD_BPX_PRO/examples/Adafruit_ST7735_PanelCheck/Adafruit_ST7735_PanelCheck.ino
 * [新增] 官方 Adafruit ST7735 + GFX 最小實機檢驗程式。
 * 與主專案獨立開啟、獨立編譯燒錄，可區分 LCD 硬體問題和 BRD BPX 業務程式。
 * TFT: 0.96 inch ST7735S, 80x160 原生 / 160x80 橫向。
 * CS 接地，RESET 10k 到 3.3V + 1uF 到 GND。
 */
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_DC 2
#define TFT_BACKLIGHT 8
#define TFT_SCK 21
#define TFT_MOSI 20
#define TFT_CS (-1)
#define TFT_RST (-1)

Adafruit_ST7735 display(&SPI, TFT_CS, TFT_DC, TFT_RST);

void setup() {
    digitalWrite(TFT_BACKLIGHT, LOW);
    pinMode(TFT_BACKLIGHT, OUTPUT);
    delay(180);
    SPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
    display.initR(INITR_MINI160x80_PLUGIN);
    delay(80);
    display.initR(INITR_MINI160x80_PLUGIN);
    display.setSPISpeed(8000000);
    display.setRotation(1);
    display.invertDisplay(true);
    display.setTextWrap(false);

    display.fillScreen(ST77XX_BLACK);
    display.drawRect(0, 0, 160, 80, ST77XX_WHITE);
    display.fillRect(2, 2, 12, 12, ST77XX_RED);
    display.fillRect(146, 2, 12, 12, ST77XX_GREEN);
    display.fillRect(2, 66, 12, 12, ST77XX_BLUE);
    display.fillRect(146, 66, 12, 12, ST77XX_YELLOW);
    display.setCursor(18, 16);
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);
    display.print("ST7735");
    display.setCursor(18, 40);
    display.setTextSize(1);
    display.setTextColor(ST77XX_CYAN);
    display.print("160x80 Adafruit GFX");
    display.drawFastHLine(20, 56, 120, ST77XX_MAGENTA);
    digitalWrite(TFT_BACKLIGHT, HIGH);
}
void loop() {
    delay(1000);
}
