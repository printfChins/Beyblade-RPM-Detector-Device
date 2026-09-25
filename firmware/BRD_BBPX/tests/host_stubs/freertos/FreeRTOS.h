/* 檔案位置: BRD_BBP/tests/host_stubs/freertos/FreeRTOS.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include <cstdint>
#define pdTRUE 1
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef struct HostQueue StaticQueue_t;
typedef HostQueue* QueueHandle_t;
