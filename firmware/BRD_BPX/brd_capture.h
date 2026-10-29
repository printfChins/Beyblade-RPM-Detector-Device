/* [新增] 檔案位置：BRD_BPX/brd_capture.h。原生 IRAM GPIO 擷取介面。 */
#ifndef BRD_CAPTURE_H
#define BRD_CAPTURE_H
#include <Arduino.h>
using brd_capture_handler_t = void (*)(void);
bool brd_capture_prepare(void);
bool brd_capture_attach_rpm(brd_capture_handler_t handler);
bool brd_capture_attach_load(brd_capture_handler_t handler);
void brd_capture_detach_rpm(void);
void brd_capture_detach_load(void);
/* [新增] IRAM 屬性只放在定義，避免 ESP_ATTR 的 section 編號重複宣告衝突。 */
uint32_t brd_capture_time_us(void);
#endif
