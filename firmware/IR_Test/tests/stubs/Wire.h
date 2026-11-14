#ifndef TEST_WIRE_H
#define TEST_WIRE_H
#include <cstddef>
#include <cstdint>
class WireMock {
public:
    void begin(int, int) {}
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t) {}
    size_t write(uint8_t) { return 1U; }
    size_t write(const uint8_t *, size_t n) { return n; }
    int endTransmission() { return 0; }
};
inline WireMock Wire;
#endif
