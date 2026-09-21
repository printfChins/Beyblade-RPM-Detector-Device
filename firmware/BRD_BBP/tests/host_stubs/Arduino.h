/* 檔案位置: BRD_BBP/tests/host_stubs/Arduino.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include <cstdint>
#include <cstddef>
#define HIGH 1
#define LOW 0
#define INPUT 1
#define INPUT_PULLUP 2
#define FALLING 3
#define CHANGE 4
#define IRAM_ATTR
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
extern uint32_t host_micros;
extern uint32_t host_millis;
extern void (*host_interrupts[32])();
inline uint32_t micros(){ return host_micros; }
inline uint32_t millis(){ return host_millis; }
inline int digitalPinToInterrupt(int p){ return p; }
inline int host_interrupt_modes[32] = {};
inline void attachInterrupt(int p, void (*f)(), int mode){ host_interrupts[p]=f; host_interrupt_modes[p]=mode; }
inline void detachInterrupt(int p){ host_interrupts[p]=nullptr; }
