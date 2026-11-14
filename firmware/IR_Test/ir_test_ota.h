/*
 * 檔案：IR_Test_OTA/ir_test_ota.h
 * [新增] BLE OTA 接口；主程式只需 begin/update、電壓與 OLED 狀態接口。
 */
#ifndef IR_TEST_OTA_H
#define IR_TEST_OTA_H

#include <stdint.h>
#include "brd_ota_protocol.h"

struct IrTestOtaDisplay {
    bool active;
    bool verifying;
    brd_ota::Phase phase;
    uint8_t percent;
};

/* [新增] 在 setup() 完成 GPIO / ADC / OLED 初始化後呼叫。 */
bool irTestOtaBegin();
/* [新增] 在 loop() 呼叫；callback 不進行 Flash 寫入。 */
void irTestOtaUpdate();
void irTestOtaSetBatteryMv(uint32_t batteryMv);
bool irTestOtaBusy();
IrTestOtaDisplay irTestOtaGetDisplay();

/* [新增] 實作於 IR_Test_OTA.ino，在 OTA 同步驗證前立即輸出 OLED VERIFY。 */
void irTestOtaRenderNow();

#endif
