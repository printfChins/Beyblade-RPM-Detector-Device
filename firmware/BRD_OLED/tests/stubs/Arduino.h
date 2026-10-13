#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_IDF_TARGET_ESP32C3 1
#define IRAM_ATTR
#define LOW 0
#define HIGH 1
#define INPUT 1
#define INPUT_PULLUP 2
#define OUTPUT 3
#define CHANGE 3
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
extern uint64_t test_clock_us;
extern void (*test_interrupts[32])(void);
inline uint32_t micros() { return static_cast<uint32_t>(test_clock_us); }
inline uint32_t millis() { return static_cast<uint32_t>(test_clock_us / 1000U); }
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int pin, void (*callback)(), int) { test_interrupts[pin] = callback; }
inline void detachInterrupt(int pin) { test_interrupts[pin] = nullptr; }
inline void delay(uint32_t ms) { test_clock_us += static_cast<uint64_t>(ms) * 1000U; }
inline void pinMode(int,int) {}
inline bool setCpuFrequencyMhz(uint32_t) { return true; }
#endif
