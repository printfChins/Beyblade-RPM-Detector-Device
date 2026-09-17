#pragma once
#include <Arduino.h>
#include <driver/gpio.h>
using i2c_master_bus_handle_t = void*;
using i2c_master_dev_handle_t = void*;
struct i2c_master_bus_config_t { int i2c_port; gpio_num_t sda_io_num; gpio_num_t scl_io_num; int clk_source; unsigned glitch_ignore_cnt; struct { bool enable_internal_pullup; } flags; };
struct i2c_device_config_t { int dev_addr_length; unsigned device_address; unsigned scl_speed_hz; };
#define I2C_NUM_0 0
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
extern int test_i2c_error, test_i2c_count;
inline int i2c_new_master_bus(const i2c_master_bus_config_t*, i2c_master_bus_handle_t *bus) { *bus=reinterpret_cast<void*>(1); return 0; }
inline int i2c_master_bus_reset(i2c_master_bus_handle_t) { return 0; }
inline int i2c_master_bus_add_device(i2c_master_bus_handle_t, const i2c_device_config_t*, i2c_master_dev_handle_t *dev) { *dev=reinterpret_cast<void*>(1); return 0; }
inline int i2c_master_probe(i2c_master_bus_handle_t, unsigned, int) { return test_i2c_error; }
inline int i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t*, size_t, int) { test_i2c_count++; return test_i2c_error; }
