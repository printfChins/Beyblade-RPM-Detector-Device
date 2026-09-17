#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#define CONFIG_IDF_TARGET_ESP32C3 1
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 2
#define INPUT_PULLUP 3
#define FALLING 4
#define CHANGE 5
#define IRAM_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
extern uint64_t test_us;
extern void (*callbacks[32])();
extern int test_pin_modes[32];
inline uint32_t micros() { return static_cast<uint32_t>(test_us); }
inline uint32_t millis() { return static_cast<uint32_t>(test_us / 1000); }
inline void delay(uint32_t ms) { test_us += static_cast<uint64_t>(ms) * 1000; }
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int pin, void (*fn)(), int) { callbacks[pin] = fn; }
inline void detachInterrupt(int pin) { callbacks[pin] = nullptr; }
inline void pinMode(int pin, int mode) { test_pin_modes[pin] = mode; }
inline bool setCpuFrequencyMhz(uint32_t mhz) { return mhz==80; }
struct MockESP {
    uint64_t getEfuseMac() const { return 0x01234567ABCDULL; }
};
inline MockESP ESP;
