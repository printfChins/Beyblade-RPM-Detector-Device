#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H
#include <deque>
#include <vector>
#include <cstring>
struct TestQueue { size_t capacity=0, item_size=0; std::deque<std::vector<uint8_t>> items; };
struct StaticQueue_t { TestQueue queue; };
using QueueHandle_t = TestQueue *;
/* [新增] 單次 dequeue 邊界注入，重現 callback 與主 loop 的交錯。 */
inline void (*test_queue_receive_hook)() = nullptr;
inline QueueHandle_t xQueueCreateStatic(size_t capacity,size_t size,uint8_t *,StaticQueue_t *control) {
    control->queue.capacity=capacity; control->queue.item_size=size; control->queue.items.clear();
    return &control->queue;
}
inline void xQueueReset(QueueHandle_t q) { q->items.clear(); }
inline int xQueueSend(QueueHandle_t q,const void *data,int) {
    if(q->items.size() >= q->capacity) { return 0; }
    const auto *bytes=static_cast<const uint8_t *>(data);
    q->items.emplace_back(bytes,bytes+q->item_size); return 1;
}
inline int xQueueReceive(QueueHandle_t q,void *data,int) {
    if (test_queue_receive_hook != nullptr) {
        auto hook = test_queue_receive_hook;
        test_queue_receive_hook = nullptr;
        hook();
    }
    if(q->items.empty()) { return 0; }
    std::memcpy(data,q->items.front().data(),q->item_size); q->items.pop_front(); return 1;
}
#endif
