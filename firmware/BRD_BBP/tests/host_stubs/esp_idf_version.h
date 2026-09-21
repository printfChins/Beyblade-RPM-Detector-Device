/* 檔案位置: BRD_BBP/tests/host_stubs/esp_idf_version.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#define ESP_IDF_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,3,0)
