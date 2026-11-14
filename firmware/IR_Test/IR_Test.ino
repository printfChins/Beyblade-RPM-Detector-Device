#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

/* [OTA 新增] 參考 BRD_BPX V1.2 的 BLE OTA 協定模組。 */
#include "ir_test_ota.h"

#define LED_GPIO                       8
#define IR1_GPIO                       3
#define IR2_GPIO                       1

/* [新增] ADC 測試腳位。 */
#define ADC_GPIO                       0

/*
 * [新增] 電池 ADC 分壓設定。
 *
 * BAT+ ---- 470k ----+---- GPIO0 / ADC
 *                    |
 *                   470k
 *                    |
 *                   GND
 *
 * VBAT = VADC * (R_TOP + R_BOTTOM) / R_BOTTOM
 *      = VADC * 2
 */
#define BAT_DIVIDER_R_TOP_OHM          470000UL
#define BAT_DIVIDER_R_BOTTOM_OHM       470000UL

/* [新增] IR 觸發電位。若實機黑白顯示相反，只需改成 LOW。 */
#define IR_TRIGGER_LEVEL               HIGH

/* [新增] OLED / I2C 設定。 */
#define OLED_SDA_GPIO                  20
#define OLED_SCL_GPIO                  21
#define OLED_I2C_ADDRESS               0x3C
#define OLED_I2C_FREQUENCY_HZ          400000UL
#define OLED_WIDTH                     128U
#define OLED_HEIGHT                    32U
#define OLED_PAGE_COUNT                (OLED_HEIGHT / 8U)
#define OLED_BUFFER_SIZE               ((OLED_WIDTH * OLED_HEIGHT) / 8U)
#define OLED_UPDATE_INTERVAL_MS        50UL
#define OLED_I2C_DATA_CHUNK_SIZE       16U

/*
 * [新增] ADC 穩定取樣參數。
 * 470k / 470k 分壓的等效源阻抗很高，先丟棄數次轉換，
 * 再以較慢節奏做多次平均，避免第一次取樣偏低。
 */
#define ADC_DISCARD_SAMPLES             8U
#define ADC_AVERAGE_SAMPLES             64U
#define ADC_SAMPLE_DELAY_US             100U

#define digitalToggle(x) digitalWrite(x, !digitalRead(x))

/* [新增] OLED framebuffer。 */
static uint8_t g_oled_buffer[OLED_BUFFER_SIZE];
static bool g_oled_ready = false;
static uint32_t g_oled_last_update_ms = 0UL;
/* [OTA 新增] OTA 模式不每 50ms 量測 72 次 ADC，降低 Flash 更新期間 CPU 負載。 */
static uint32_t g_battery_last_update_ms = 0UL;
static uint32_t g_battery_cached_mv = 0UL;

int ls_IR1 = -1;
int ls_IR2 = -1;

/* =========================================================
 * [新增] SSD1306 OLED 基礎驅動
 * ========================================================= */
static bool oledWriteCommands(const uint8_t *commands, size_t length) {
  if ((commands == nullptr) || (length == 0U)) {
    return false;
  }

  Wire.beginTransmission(OLED_I2C_ADDRESS);
  Wire.write(0x00U);
  Wire.write(commands, length);

  return (Wire.endTransmission() == 0U);
}

static bool oledWriteData(const uint8_t *data, size_t length) {
  size_t offset = 0U;

  if ((data == nullptr) || (length == 0U)) {
    return false;
  }

  while (offset < length) {
    size_t chunkLength = length - offset;

    if (chunkLength > OLED_I2C_DATA_CHUNK_SIZE) {
      chunkLength = OLED_I2C_DATA_CHUNK_SIZE;
    }

    Wire.beginTransmission(OLED_I2C_ADDRESS);
    Wire.write(0x40U);
    Wire.write(&data[offset], chunkLength);

    if (Wire.endTransmission() != 0U) {
      return false;
    }

    offset += chunkLength;
  }

  return true;
}

static bool oledProbe(void) {
  Wire.beginTransmission(OLED_I2C_ADDRESS);
  return (Wire.endTransmission() == 0U);
}

static void oledClearBuffer(void) {
  memset(g_oled_buffer, 0, sizeof(g_oled_buffer));
}

static void oledSetPixel(uint8_t x, uint8_t y, bool on) {
  uint16_t index;
  uint8_t mask;

  if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT)) {
    return;
  }

  index = (uint16_t)x + ((uint16_t)(y / 8U) * OLED_WIDTH);
  mask = (uint8_t)(1U << (y & 0x07U));

  if (on == true) {
    g_oled_buffer[index] |= mask;
  } else {
    g_oled_buffer[index] &= (uint8_t)(~mask);
  }
}

static void oledDrawHorizontalLine(uint8_t x, uint8_t y, uint8_t width) {
  uint16_t i;

  for (i = 0U; i < width; i++) {
    oledSetPixel((uint8_t)(x + i), y, true);
  }
}

static void oledDrawVerticalLine(uint8_t x, uint8_t y, uint8_t height) {
  uint16_t i;

  for (i = 0U; i < height; i++) {
    oledSetPixel(x, (uint8_t)(y + i), true);
  }
}

static void oledDrawRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  if ((width < 2U) || (height < 2U)) {
    return;
  }

  oledDrawHorizontalLine(x, y, width);
  oledDrawHorizontalLine(x, (uint8_t)(y + height - 1U), width);
  oledDrawVerticalLine(x, y, height);
  oledDrawVerticalLine((uint8_t)(x + width - 1U), y, height);
}

static void oledFillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  uint16_t px;
  uint16_t py;

  for (py = 0U; py < height; py++) {
    for (px = 0U; px < width; px++) {
      oledSetPixel((uint8_t)(x + px), (uint8_t)(y + py), true);
    }
  }
}

/*
 * [新增] 5x7 字型。
 * 測試畫面只保留需要的字元：0~9、A/C/D/I/R/V、空白、小數點。
 */
static const uint8_t *oledGetGlyph(char c) {
  static const uint8_t glyphSpace[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t glyphDot[5]   = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t glyph0[5]     = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t glyph1[5]     = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t glyph2[5]     = {0x42, 0x61, 0x51, 0x49, 0x46};
  static const uint8_t glyph3[5]     = {0x21, 0x41, 0x45, 0x4B, 0x31};
  static const uint8_t glyph4[5]     = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t glyph5[5]     = {0x27, 0x45, 0x45, 0x45, 0x39};
  static const uint8_t glyph6[5]     = {0x3C, 0x4A, 0x49, 0x49, 0x30};
  static const uint8_t glyph7[5]     = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t glyph8[5]     = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t glyph9[5]     = {0x06, 0x49, 0x49, 0x29, 0x1E};
  static const uint8_t glyphA[5]     = {0x7E, 0x11, 0x11, 0x11, 0x7E};
  /* [新增] BAT 顯示需要 B / T。 */
  static const uint8_t glyphB[5]     = {0x7F, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t glyphC[5]     = {0x3E, 0x41, 0x41, 0x41, 0x22};
  static const uint8_t glyphD[5]     = {0x7F, 0x41, 0x41, 0x22, 0x1C};
  static const uint8_t glyphI[5]     = {0x00, 0x41, 0x7F, 0x41, 0x00};
  static const uint8_t glyphR[5]     = {0x7F, 0x09, 0x19, 0x29, 0x46};
  /* [新增] BAT 顯示需要 T。 */
  static const uint8_t glyphT[5]     = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t glyphV[5]     = {0x1F, 0x20, 0x40, 0x20, 0x1F};
  /* [OTA 新增] 限定增加 OTA UPDATE / VERIFY / DONE / ERROR / % 所需字元。 */
  static const uint8_t glyphE[5]     = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t glyphF[5]     = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t glyphN[5]     = {0x7F, 0x02, 0x0C, 0x10, 0x7F};
  static const uint8_t glyphO[5]     = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t glyphP[5]     = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t glyphU[5]     = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t glyphY[5]     = {0x07, 0x08, 0x70, 0x08, 0x07};
  static const uint8_t glyphPercent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};

  switch (c) {
    case '0': return glyph0;
    case '1': return glyph1;
    case '2': return glyph2;
    case '3': return glyph3;
    case '4': return glyph4;
    case '5': return glyph5;
    case '6': return glyph6;
    case '7': return glyph7;
    case '8': return glyph8;
    case '9': return glyph9;
    case 'A': return glyphA;
    case 'B': return glyphB;
    case 'C': return glyphC;
    case 'D': return glyphD;
    case 'E': return glyphE;
    case 'F': return glyphF;
    case 'I': return glyphI;
    case 'N': return glyphN;
    case 'O': return glyphO;
    case 'P': return glyphP;
    case 'R': return glyphR;
    case 'T': return glyphT;
    case 'U': return glyphU;
    case 'V': return glyphV;
    case 'Y': return glyphY;
    case '%': return glyphPercent;
    case '.': return glyphDot;
    case ' ':
    default:
      return glyphSpace;
  }
}

static void oledDrawChar(uint8_t x, uint8_t y, char c) {
  const uint8_t *glyph = oledGetGlyph(c);
  uint8_t column;
  uint8_t row;

  for (column = 0U; column < 5U; column++) {
    for (row = 0U; row < 7U; row++) {
      if ((glyph[column] & (1U << row)) != 0U) {
        oledSetPixel((uint8_t)(x + column), (uint8_t)(y + row), true);
      }
    }
  }
}

static void oledDrawText(uint8_t x, uint8_t y, const char *text) {
  uint8_t cursorX = x;

  if (text == nullptr) {
    return;
  }

  while (*text != '\0') {
    oledDrawChar(cursorX, y, *text);
    cursorX = (uint8_t)(cursorX + 6U);
    text++;
  }
}

/*
 * [OTA 新增] 與參考 BRD_BPX 相同的 OLED 扇形圓盤及兩倍字級百分比。
 * 只使用原有的 5x7 glyph，% 使用 10x14 空心圓圈筆畫，不新增函式庫。
 */
static void oledDrawCharScaled2(uint8_t x, uint8_t y, char c) {
  if (c == '%') {
    /* [OTA 沿用] 參考 BRD_BPX 的 10x14 專用空心 % glyph。 */
    static const uint16_t rows[14] = {
      0x206U, 0x209U, 0x109U, 0x106U,
      0x080U, 0x080U, 0x040U, 0x020U,
      0x020U, 0x010U, 0x188U, 0x248U,
      0x244U, 0x184U
    };
    for (uint8_t row = 0U; row < 14U; row++) {
      for (uint8_t column = 0U; column < 10U; column++) {
        if ((rows[row] & (1U << column)) != 0U) {
          oledSetPixel((uint8_t)(x + column), (uint8_t)(y + row), true);
        }
      }
    }
    return;
  }
  const uint8_t *glyph = oledGetGlyph(c);
  for (uint8_t column = 0U; column < 5U; column++) {
    for (uint8_t row = 0U; row < 7U; row++) {
      if ((glyph[column] & (1U << row)) != 0U) {
        for (uint8_t dx = 0U; dx < 2U; dx++) {
          for (uint8_t dy = 0U; dy < 2U; dy++) {
            oledSetPixel((uint8_t)(x + column * 2U + dx),
                         (uint8_t)(y + row * 2U + dy), true);
          }
        }
      }
    }
  }
}

static void oledDrawTextScaled2(uint8_t x, uint8_t y, const char *text) {
  while (text != nullptr && *text != '\0') {
    oledDrawCharScaled2(x, y, *text);
    x = (uint8_t)(x + 12U);
    text++;
  }
}

static void oledDrawOtaSector(uint8_t cx, uint8_t cy, uint8_t radius, uint8_t percent) {
  if (percent > 100U) {
    percent = 100U;
  }
  const int16_t r = (int16_t)radius;
  const int16_t outer = r * r;
  const int16_t inner = (r - 1) * (r - 1);

  for (int16_t dy = -r; dy <= r; dy++) {
    for (int16_t dx = -r; dx <= r; dx++) {
      int16_t dist = dx * dx + dy * dy;
      if (dist > outer) {
        continue;
      }
      bool filled = (dist >= inner);
      if (percent == 100U || (percent != 0U && dx == 0 && dy == 0)) {
        filled = true;
      } else if (percent != 0U) {
        uint8_t quarter;
        int16_t numerator;
        int16_t denominator;
        if (dx >= 0 && dy < 0) {
          quarter = 0U;
          numerator = dx;
          denominator = dx - dy;
        } else if (dx > 0 && dy >= 0) {
          quarter = 1U;
          numerator = dy;
          denominator = dx + dy;
        } else if (dx <= 0 && dy > 0) {
          quarter = 2U;
          numerator = -dx;
          denominator = -dx + dy;
        } else {
          quarter = 3U;
          numerator = -dy;
          denominator = -dx - dy;
        }
        if ((uint32_t)percent * (uint32_t)denominator >=
            (uint32_t)quarter * 25U * (uint32_t)denominator +
            25U * (uint32_t)numerator) {
          filled = true;
        }
      }
      if (filled) {
        oledSetPixel((uint8_t)((int16_t)cx + dx),
                     (uint8_t)((int16_t)cy + dy), true);
      }
    }
  }
}

static bool oledFlush(void) {
  const uint8_t setAddressCommands[] = {
    0x21U, 0x00U, (uint8_t)(OLED_WIDTH - 1U),
    0x22U, 0x00U, (uint8_t)(OLED_PAGE_COUNT - 1U)
  };

  if (oledWriteCommands(setAddressCommands, sizeof(setAddressCommands)) == false) {
    return false;
  }

  return oledWriteData(g_oled_buffer, sizeof(g_oled_buffer));
}

static bool oledBegin(void) {
  const uint8_t initCommands[] = {
    0xAEU,
    0xD5U, 0x80U,
    0xA8U, 0x1FU,
    0xD3U, 0x00U,
    0x40U,
    0x8DU, 0x14U,
    0x20U, 0x00U,
    /* [修改] OLED 旋轉 180 度：SEG remap normal + COM scan normal。 */
    0xA0U,
    0xC0U,
    0xDAU, 0x02U,
    0x81U, 0x8FU,
    0xD9U, 0xF1U,
    0xDBU, 0x40U,
    0xA4U,
    0xA6U,
    0xAFU
  };

  Wire.begin(OLED_SDA_GPIO, OLED_SCL_GPIO);
  Wire.setClock(OLED_I2C_FREQUENCY_HZ);

  if (oledProbe() == false) {
    return false;
  }

  if (oledWriteCommands(initCommands, sizeof(initCommands)) == false) {
    return false;
  }

  oledClearBuffer();

  if (oledFlush() == false) {
    return false;
  }

  return true;
}

/* =========================================================
 * [新增] ADC 與測試畫面
 * ========================================================= */
static uint32_t readAdcMilliVolts(void) {
  uint64_t totalMilliVolts = 0ULL;
  uint8_t i;

  /*
   * [修改]
   * 先做數次 dummy read，讓 ADC 前端與外部分壓節點穩定。
   */
  for (i = 0U; i < ADC_DISCARD_SAMPLES; i++) {
    (void)analogReadMilliVolts(ADC_GPIO);
    delayMicroseconds(ADC_SAMPLE_DELAY_US);
  }

  /*
   * [修改]
   * 使用 64 次 calibrated mV 取樣平均。
   */
  for (i = 0U; i < ADC_AVERAGE_SAMPLES; i++) {
    totalMilliVolts += (uint32_t)analogReadMilliVolts(ADC_GPIO);
    delayMicroseconds(ADC_SAMPLE_DELAY_US);
  }

  return (uint32_t)(totalMilliVolts / ADC_AVERAGE_SAMPLES);
}

/*
 * [OTA 校正註解] 將 GPIO0 ADC 電壓換算回電池端電壓，算法不修改。
 *
 * 470k / 470k：
 * VBAT = VADC * 2
 *
 * 使用 uint64_t 避免乘法中間值溢位。
 */
static uint32_t readBatteryMilliVolts(void) {
  uint32_t adcMilliVolts;
  uint64_t batteryMilliVolts;

  adcMilliVolts = readAdcMilliVolts();

  batteryMilliVolts =
      ((uint64_t)adcMilliVolts *
       (BAT_DIVIDER_R_TOP_OHM + BAT_DIVIDER_R_BOTTOM_OHM)) /
      BAT_DIVIDER_R_BOTTOM_OHM;

  return (uint32_t)batteryMilliVolts;
}

/*
 * [OTA 新增] ADC 週期更新與 OLED 有無初始化成功解耦。
 * 一般模式仍每 50ms 取 72 筆（8 dummy + 64 average）；
 * OTA 期間每 1000ms 更新，避免 Flash 傳輸時被 ADC 長時間占用。
 */
static void batteryUpdate(void) {
  uint32_t nowMs = millis();
  uint32_t intervalMs = irTestOtaBusy() ? 1000UL : OLED_UPDATE_INTERVAL_MS;
  if ((uint32_t)(nowMs - g_battery_last_update_ms) < intervalMs) {
    return;
  }
  g_battery_cached_mv = readBatteryMilliVolts();
  g_battery_last_update_ms = millis();
  irTestOtaSetBatteryMv(g_battery_cached_mv);
}

static void oledDrawIrState(uint8_t x, uint8_t y, bool triggered) {
  const uint8_t boxSize = 12U;

  if (triggered == true) {
    /* [需求] 全黑方塊 = IR 觸發。 */
    oledFillRect(x, y, boxSize, boxSize);
  } else {
    /* [需求] 空心方塊 = IR 未觸發。 */
    oledDrawRect(x, y, boxSize, boxSize);
  }
}

/* =========================================================
 * [OTA 新增] 原來 OLED 的 IR/ADC 顯示原樣保留；OTA 時改為專用進度畫面。
 * ========================================================= */
static void oledDrawOtaDisplay(const IrTestOtaDisplay &ota) {
  char percentText[8];

  oledClearBuffer();
  if (ota.phase == brd_ota::COMPLETE) {
    oledDrawText(0U, 1U, "OTA DONE");
  } else if (ota.verifying) {
    oledDrawText(0U, 1U, "VERIFY");
  } else if (ota.phase == brd_ota::ERROR) {
    oledDrawText(0U, 1U, "OTA ERROR");
  } else {
    oledDrawText(0U, 1U, "OTA UPDATE");
  }

  /* [OTA 沿用] 17x17 左扇形圓盤，中央大字百分比，完全對齊 BPX 的 UI。 */
  oledDrawOtaSector(13U, 23U, 8U, ota.percent);
  snprintf(percentText, sizeof(percentText), "%u%%", (unsigned)ota.percent);
  uint8_t textWidth = (uint8_t)(strlen(percentText) * 12U);
  uint8_t textX = (uint8_t)((OLED_WIDTH - textWidth) / 2U);
  oledDrawTextScaled2(textX, 16U, percentText);
  (void)oledFlush();
}

static void oledUpdate(int ir1, int ir2, bool force = false) {
  uint32_t nowMs = millis();
  uint32_t batteryMilliVolts;
  bool ir1Triggered;
  bool ir2Triggered;
  char batteryText[16];
  IrTestOtaDisplay ota;

  if (g_oled_ready == false) {
    return;
  }

  /* [OTA 修改] 保留原本每 50ms 更新 OLED 的速率。 */
  if (!force && (uint32_t)(nowMs - g_oled_last_update_ms) < OLED_UPDATE_INTERVAL_MS) {
    return;
  }
  g_oled_last_update_ms = nowMs;

  ota = irTestOtaGetDisplay();

  if (ota.active) {
    oledDrawOtaDisplay(ota);
    return;
  }

  ir1Triggered = (ir1 == IR_TRIGGER_LEVEL);
  ir2Triggered = (ir2 == IR_TRIGGER_LEVEL);
  batteryMilliVolts = g_battery_cached_mv;

  snprintf(batteryText,
           sizeof(batteryText),
           "BAT %lu.%03luV",
           (unsigned long)(batteryMilliVolts / 1000UL),
           (unsigned long)(batteryMilliVolts % 1000UL));

  oledClearBuffer();

  /* [保留] 第一列：IR1 / IR2 原本的觸發狀態及方塊。 */
  oledDrawText(0U, 3U, "IR1");
  oledDrawIrState(20U, 1U, ir1Triggered);

  oledDrawText(48U, 3U, "IR2");
  oledDrawIrState(68U, 1U, ir2Triggered);

  /* [保留] 第二列：原本的電池端電壓，未修改分壓倍率及小數位數。 */
  oledDrawText(0U, 21U, batteryText);

  (void)oledFlush();
}

/* [OTA 新增] 由 Flash 驗證流程同步呼叫，立即讓使用者看到 VERIFY。 */
void irTestOtaRenderNow() {
  oledUpdate(digitalRead(IR1_GPIO), digitalRead(IR2_GPIO), true);
}

void setup() {
  pinMode(LED_GPIO, OUTPUT);
  pinMode(IR1_GPIO, INPUT);
  pinMode(IR2_GPIO, INPUT);
  /* [新增] 明確設定 ADC GPIO 為輸入。 */
  pinMode(ADC_GPIO, INPUT);

  digitalWrite(LED_GPIO, LOW);

  /* [新增] ADC 設定：12-bit、11 dB attenuation。 */
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_GPIO, ADC_11db);

  /* [新增] 初始化 OLED。 */
  g_oled_ready = oledBegin();
  g_oled_last_update_ms = 0UL;

  /* [OTA 新增] BLE 初始化前先取得有效電池電壓，不允許低電誤啟動 OTA。 */
  g_battery_cached_mv = readBatteryMilliVolts();
  g_battery_last_update_ms = millis();
  irTestOtaSetBatteryMv(g_battery_cached_mv);

  /* [OTA 新增] 與 IR 獨立的 BLE OTA，沒有引入 BBP、RPM、NVS 驅動。 */
  (void)irTestOtaBegin();
}

void loop() {
  int IR1 = digitalRead(IR1_GPIO);
  int IR2 = digitalRead(IR2_GPIO);

  /* [OTA 新增] 先更新電壓安全判斷，再處理入列的 OTA BEGIN。 */
  batteryUpdate();

  /* [OTA 新增] BLE 回呼只入列；本機 loop 優先處理傳輸/校驗。 */
  irTestOtaUpdate();

  /* [OTA 修改] OTA 中不切換 IR LED；非 OTA 時保留原本狀態變化邏輯。 */
  if (!irTestOtaBusy() && (IR1 != ls_IR1 || IR2 != ls_IR2)) {
    digitalToggle(LED_GPIO);
    ls_IR1 = IR1;
    ls_IR2 = IR2;
  }

  /* [修改] 更新 OLED：IR 方塊 + 電池端電壓。 */
  oledUpdate(IR1, IR2);

  /* [OTA 修改] OTA 期間縮短 loop delay，改善 GATT 封包處理能力。 */
  delay(irTestOtaBusy() ? 1 : 10);
}
