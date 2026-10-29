/* [PRO 新增] Host SPI mock 僅用於繪圖與編譯測試。 */
#ifndef TEST_SPI_H
#define TEST_SPI_H
#include <cstdint>
#include <cstddef>
#define MSBFIRST 1
#define SPI_MODE0 0
class SPISettings {
public:
    SPISettings(uint32_t,uint8_t,uint8_t) {}
};
class SPIClass {
public:
    void begin(int,int,int,int) {}
    void beginTransaction(const SPISettings &) {}
    void write(uint8_t) {}
    void writeBytes(const uint8_t *,uint32_t) {}
    void endTransaction() {}
};
[[maybe_unused]] static SPIClass SPI;
#endif
