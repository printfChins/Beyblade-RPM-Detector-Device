/*
    檔案位置: BRD_BPX/brd_settings.h
    [新增] RAM / Flash 設定入口，全部由 setup 或主 loop 呼叫。
*/
#ifndef BRD_SETTINGS_H
#define BRD_SETTINGS_H
#include "brd_settings_model.h"
void brd_settings_begin(void);
const brd_settings::Values &brd_settings_get(void);
const brd_settings::Manager &brd_settings_model(void);
brd_settings::Status brd_settings_process_request(const uint8_t *data, size_t length,
                                                  uint8_t out[brd_settings::RESPONSE_SIZE]);
void brd_settings_discard_draft(void);
void brd_settings_update(void);
bool brd_settings_take_completion(uint8_t out[brd_settings::RESPONSE_SIZE]);
bool brd_settings_bluetooth_allowed(bool charging);
#endif
