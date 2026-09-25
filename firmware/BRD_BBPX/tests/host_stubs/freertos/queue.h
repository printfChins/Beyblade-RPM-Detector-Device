/* 檔案位置: BRD_BBP/tests/host_stubs/freertos/queue.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>
struct HostQueue{size_t cap=0,item=0;std::deque<std::vector<uint8_t>> q;};
inline QueueHandle_t xQueueCreateStatic(UBaseType_t c,UBaseType_t s,uint8_t*,StaticQueue_t*b){b->cap=c;b->item=s;return b;}
inline BaseType_t xQueueSend(QueueHandle_t q,const void*p,int){if(q->q.size()>=q->cap)return 0;q->q.emplace_back((const uint8_t*)p,(const uint8_t*)p+q->item);return pdTRUE;}
inline BaseType_t xQueueReceive(QueueHandle_t q,void*p,int){if(q->q.empty())return 0;std::memcpy(p,q->q.front().data(),q->item);q->q.pop_front();return pdTRUE;}
inline void xQueueReset(QueueHandle_t q){q->q.clear();}
