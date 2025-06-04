#include <stdio.h>
#include "esp_log.h"
#include "codec_init.h"
#include "codec_board.h"
#include "esp_codec_dev.h"
#include "sdkconfig.h"
#include "settings.h"
#include "driver/i2c.h"
#include <string.h>

static const char *TAG = "Board";

/* I2C configuration for the MUSE_RADIO board */
#define MUSE_I2C_PORT      0
#define MUSE_I2C_SCL       11
#define MUSE_I2C_SDA       18
#define ES8388_ADDR        0x10

/* ES8388 register map */
#define ES8388_CONTROL1        0x00
#define ES8388_CONTROL2        0x01
#define ES8388_CHIPPOWER       0x02
#define ES8388_ADCPOWER        0x03
#define ES8388_DACPOWER        0x04
#define ES8388_CHIPLOPOW1      0x05
#define ES8388_CHIPLOPOW2      0x06
#define ES8388_ANAVOLMANAG     0x07
#define ES8388_MASTERMODE      0x08

/* ADC registers */
#define ES8388_ADCCONTROL1     0x09
#define ES8388_ADCCONTROL2     0x0a
#define ES8388_ADCCONTROL3     0x0b
#define ES8388_ADCCONTROL4     0x0c
#define ES8388_ADCCONTROL5     0x0d
#define ES8388_ADCCONTROL6     0x0e
#define ES8388_ADCCONTROL7     0x0f
#define ES8388_ADCCONTROL8     0x10
#define ES8388_ADCCONTROL9     0x11
#define ES8388_ADCCONTROL10    0x12
#define ES8388_ADCCONTROL11    0x13
#define ES8388_ADCCONTROL12    0x14
#define ES8388_ADCCONTROL13    0x15
#define ES8388_ADCCONTROL14    0x16

/* DAC registers */
#define ES8388_DACCONTROL1     0x17
#define ES8388_DACCONTROL2     0x18
#define ES8388_DACCONTROL3     0x19
#define ES8388_DACCONTROL4     0x1a
#define ES8388_DACCONTROL5     0x1b
#define ES8388_DACCONTROL6     0x1c
#define ES8388_DACCONTROL7     0x1d
#define ES8388_DACCONTROL8     0x1e
#define ES8388_DACCONTROL9     0x1f
#define ES8388_DACCONTROL10    0x20
#define ES8388_DACCONTROL11    0x21
#define ES8388_DACCONTROL12    0x22
#define ES8388_DACCONTROL13    0x23
#define ES8388_DACCONTROL14    0x24
#define ES8388_DACCONTROL15    0x25
#define ES8388_DACCONTROL16    0x26
#define ES8388_DACCONTROL17    0x27
#define ES8388_DACCONTROL18    0x28
#define ES8388_DACCONTROL19    0x29
#define ES8388_DACCONTROL20    0x2a
#define ES8388_DACCONTROL21    0x2b
#define ES8388_DACCONTROL22    0x2c
#define ES8388_DACCONTROL23    0x2d
#define ES8388_DACCONTROL24    0x2e
#define ES8388_DACCONTROL25    0x2f
#define ES8388_DACCONTROL26    0x30
#define ES8388_DACCONTROL27    0x31
#define ES8388_DACCONTROL28    0x32
#define ES8388_DACCONTROL29    0x33
#define ES8388_DACCONTROL30    0x34

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MUSE_I2C_SDA,
        .scl_io_num = MUSE_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(MUSE_I2C_PORT, &conf);
    return i2c_driver_install(MUSE_I2C_PORT, conf.mode, 0, 0, 0);
}

static esp_err_t write_reg(uint8_t device_address, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(MUSE_I2C_PORT, device_address, data, sizeof(data), portMAX_DELAY);
}

static esp_err_t es8388_init_regs(void)
{
    esp_err_t res = 0;

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x04);
    res |= write_reg(ES8388_ADDR, ES8388_CONTROL2, 0x50);
    res |= write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_MASTERMODE, 0x00);

    res |= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3e);
    res |= write_reg(ES8388_ADDR, ES8388_CONTROL1, 0x12);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL1, 0x18);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL2, 0x02);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL16, 0x1B);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL17, 0x90);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL20, 0x90);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL21, 0x80);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL23, 0x00);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL5, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL4, 0x00);

    res |= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0xff);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x88);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL2, 0xFC);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL3, 0x02);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x0c);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL5, 0x02);

    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL8, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL9, 0x00);

    write_reg(ES8388_ADDR, ES8388_ADCCONTROL10, 0xf8);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL11, 0x30);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL12, 0x57);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL13, 0x06);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL14, 0x89);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL7, 0x20);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL24, 0x21);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL25, 0x21);

    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x21);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x21);

    res |= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3C);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0x00);

    return res;
}

void init_board(void)
{
    ESP_LOGI(TAG, "Init board.");
    if (strcmp(TEST_BOARD_NAME, "MUSE_RADIO") == 0) {
        i2c_master_init();
        es8388_init_regs();
        i2c_driver_delete(MUSE_I2C_PORT);
    }
    set_codec_board_type(TEST_BOARD_NAME);
    // Notes when use playback and record at same time, must set reuse_dev = false
    codec_init_cfg_t cfg = {
#if CONFIG_IDF_TARGET_ESP32S3
        .in_mode = CODEC_I2S_MODE_TDM,
        .in_use_tdm = true,
#endif
        .reuse_dev = false
    };
    init_codec(&cfg);
}
