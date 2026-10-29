#ifndef TEST_I2C_H
#define TEST_I2C_H
#include <Arduino.h>
#include <esp_err.h>
using i2c_master_bus_handle_t = void *;
using i2c_master_dev_handle_t = void *;
#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
struct i2c_master_bus_config_t {
    int i2c_port=0,sda_io_num=0,scl_io_num=0,clk_source=0;
    uint8_t glitch_ignore_cnt=0;
    struct { bool enable_internal_pullup=false; } flags;
};
struct i2c_device_config_t { int dev_addr_length=0; uint16_t device_address=0; uint32_t scl_speed_hz=0; };
inline int i2c_new_master_bus(const i2c_master_bus_config_t *,void **out) { *out=reinterpret_cast<void *>(1); return ESP_OK; }
inline int i2c_master_bus_reset(void *) { return ESP_OK; }
inline int i2c_master_bus_add_device(void *,const i2c_device_config_t *,void **out) { *out=reinterpret_cast<void *>(2); return ESP_OK; }
inline int i2c_master_probe(void *,uint16_t,int) { return ESP_OK; }
inline int i2c_master_transmit(void *,const uint8_t *,size_t,int) { return ESP_OK; }
#endif
