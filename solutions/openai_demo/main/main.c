/* OpenAI realtime communication test

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_webrtc.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "esp_timer.h"
#include "esp_cpu.h"
#include "common.h"
#include "esp_capture_defaults.h"
#include "driver/i2c.h"

// I2C GPIOs
#define I2C_SCL          11
#define I2C_SDA          18
#define I2C_NUM          0
#define ES8388_ADDR      0x10

// ES8388 register address
#define ES8388_CONTROL1     0x00
#define ES8388_CONTROL2     0x01
#define ES8388_CHIPPOWER    0x02
#define ES8388_ADCPOWER     0x03
#define ES8388_DACPOWER     0x04
#define ES8388_CHIPLOPOW1   0x05
#define ES8388_CHIPLOPOW2   0x06
#define ES8388_ANAVOLMANAG  0x07
#define ES8388_MASTERMODE   0x08

// ADC registers
#define ES8388_ADCCONTROL1  0x09
#define ES8388_ADCCONTROL2  0x0a
#define ES8388_ADCCONTROL3  0x0b
#define ES8388_ADCCONTROL4  0x0c
#define ES8388_ADCCONTROL5  0x0d
#define ES8388_ADCCONTROL6  0x0e
#define ES8388_ADCCONTROL7  0x0f
#define ES8388_ADCCONTROL8  0x10
#define ES8388_ADCCONTROL9  0x11
#define ES8388_ADCCONTROL10 0x12
#define ES8388_ADCCONTROL11 0x13
#define ES8388_ADCCONTROL12 0x14
#define ES8388_ADCCONTROL13 0x15
#define ES8388_ADCCONTROL14 0x16

// DAC registers
#define ES8388_DACCONTROL1  0x17
#define ES8388_DACCONTROL2  0x18
#define ES8388_DACCONTROL3  0x19
#define ES8388_DACCONTROL4  0x1a
#define ES8388_DACCONTROL5  0x1b
#define ES8388_DACCONTROL6  0x1c
#define ES8388_DACCONTROL7  0x1d
#define ES8388_DACCONTROL8  0x1e
#define ES8388_DACCONTROL9  0x1f
#define ES8388_DACCONTROL10 0x20
#define ES8388_DACCONTROL11 0x21
#define ES8388_DACCONTROL12 0x22
#define ES8388_DACCONTROL13 0x23
#define ES8388_DACCONTROL14 0x24
#define ES8388_DACCONTROL15 0x25
#define ES8388_DACCONTROL16 0x26
#define ES8388_DACCONTROL17 0x27
#define ES8388_DACCONTROL18 0x28
#define ES8388_DACCONTROL19 0x29
#define ES8388_DACCONTROL20 0x2a
#define ES8388_DACCONTROL21 0x2b
#define ES8388_DACCONTROL22 0x2c
#define ES8388_DACCONTROL23 0x2d
#define ES8388_DACCONTROL24 0x2e
#define ES8388_DACCONTROL25 0x2f
#define ES8388_DACCONTROL26 0x30
#define ES8388_DACCONTROL27 0x31
#define ES8388_DACCONTROL28 0x32
#define ES8388_DACCONTROL29 0x33
#define ES8388_DACCONTROL30 0x34

static int start_chat(int argc, char **argv)
{
    start_webrtc();
    return 0;
}

#define RUN_ASYNC(name, body)           \
    void run_async##name(void *arg)     \
    {                                   \
        body;                           \
        media_lib_thread_destroy(NULL); \
    }                                   \
    media_lib_thread_create_from_scheduler(NULL, #name, run_async##name, NULL);

static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

static esp_err_t write_reg(uint8_t device_address, uint8_t reg, uint8_t val)
{
    uint8_t b[2];
    b[0] = reg;
    b[1] = val;
    return i2c_master_write_to_device(I2C_NUM, device_address, b, sizeof(b), portMAX_DELAY);
}

static esp_err_t ES8388_init(void)
{
    esp_err_t res = ESP_OK;

    /* mute DAC during setup, power up all systems, slave mode */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x04);
    res |= write_reg(ES8388_ADDR, ES8388_CONTROL2, 0x50);
    res |= write_reg(ES8388_ADDR, ES8388_CHIPPOWER, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_MASTERMODE, 0x00);

    /* power up DAC and enable outputs */
    res |= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3e);
    res |= write_reg(ES8388_ADDR, ES8388_CONTROL1, 0x12);

    /* DAC I2S setup: 16 bit word length, I2S format */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL1, 0x18);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL2, 0x02);

    /* Route ADC mix to output */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL16, 0x1B);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL17, 0x90);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL20, 0x90);

    /* Use same LRCK for DAC and ADC */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL21, 0x80);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL23, 0x00);

    /* DAC volume control: 0dB */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL5, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL4, 0x00);

    /* Configure ADC */
    res |= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0xff);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL1, 0x88);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL2, 0xFC);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL3, 0x02);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL4, 0x0c);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL5, 0x02);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL8, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_ADCCONTROL9, 0x00);

    /* ALC configuration */
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL10, 0xf8);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL11, 0x30);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL12, 0x57);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL13, 0x06);
    write_reg(ES8388_ADDR, ES8388_ADCCONTROL14, 0x89);

    /* Mono output */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL7, 0x20);

    /* Output volume 0dB */
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL24, 0x21);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL25, 0x21);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL26, 0x21);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL27, 0x21);

    /* Power up ADC and DAC */
    res |= write_reg(ES8388_ADDR, ES8388_DACPOWER, 0x3C);
    res |= write_reg(ES8388_ADDR, ES8388_DACCONTROL3, 0x00);
    res |= write_reg(ES8388_ADDR, ES8388_ADCPOWER, 0x00);

    return res;
}

static int stop_chat(int argc, char **argv)
{
    RUN_ASYNC(stop, { stop_webrtc(); });
    return 0;
}

static int assert_cli(int argc, char **argv)
{
    *(int *)0 = 0;
    return 0;
}

static int sys_cli(int argc, char **argv)
{
    sys_state_show();
    return 0;
}

static int wifi_cli(int argc, char **argv)
{
    if (argc < 1) {
        return -1;
    }
    char *ssid = argv[1];
    char *password = argc > 2 ? argv[2] : NULL;
    return network_connect_wifi(ssid, password);
}

static int measure_cli(int argc, char **argv)
{
    void measure_enable(bool enable);
    void show_measure(void);
    measure_enable(true);
    media_lib_thread_sleep(1500);
    measure_enable(false);
    return 0;
}

static int dump_cli(int argc, char **argv)
{
    bool enable = (argc > 1);
    printf("Enable AEC dump %d\n", enable);
    esp_capture_enable_aec_src_dump(enable);
    return 0;
}

static int rec2play_cli(int argc, char **argv)
{
    test_capture_to_player();
    return 0;
}

static int text_cli(int argc, char **argv)
{
    if (argc > 1) {
        openai_send_text(argv[1]);
    }
    return 0;
}

static int init_console()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp>";
    repl_config.task_stack_size = 10 * 1024;
    repl_config.task_priority = 22;
    repl_config.max_cmdline_length = 1024;
    // install console REPL environment
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif
    esp_console_cmd_t cmds[] = {
        {
            .command = "start",
            .help = "Start OpenAI realtime communication\r\n",
            .func = start_chat,
        },
        {
            .command = "stop",
            .help = "Stop chat\n",
            .func = stop_chat,
        },
        {
            .command = "i",
            .help = "Show system status\r\n",
            .func = sys_cli,
        },
        {
            .command = "assert",
            .help = "Assert system\r\n",
            .func = assert_cli,
        },
        {
            .command = "wifi",
            .help = "wifi ssid psw\r\n",
            .func = wifi_cli,
        },
        {
            .command = "m",
            .help = "measure system loading\r\n",
            .func = measure_cli,
        },
        {
            .command = "dump",
            .help = "Dump AEC data\r\n",
            .func = dump_cli,
        },
        {
            .command = "text",
            .help = "Send text message\r\n",
            .func = text_cli,
        },
        {
            .command = "rec2play",
            .help = "Play recorded voice\r\n",
            .func = rec2play_cli,
        },
    };
    for (int i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&cmds[i]));
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    return 0;
}

static void thread_scheduler(const char *thread_name, media_lib_thread_cfg_t *thread_cfg)
{
    if (strcmp(thread_name, "pc_task") == 0) {
        thread_cfg->stack_size = 25 * 1024;
        thread_cfg->priority = 18;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "start") == 0) {
        thread_cfg->stack_size = 6 * 1024;
    }
    if (strcmp(thread_name, "pc_send") == 0) {
        thread_cfg->stack_size = 4 * 1024;
        thread_cfg->priority = 15;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "Adec") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 1;
    }
    if (strcmp(thread_name, "venc") == 0) {
#if CONFIG_IDF_TARGET_ESP32S3
        thread_cfg->stack_size = 20 * 1024;
#endif
        thread_cfg->priority = 10;
    }
#ifdef WEBRTC_SUPPORT_OPUS
    if (strcmp(thread_name, "aenc") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 10;
    }
    if (strcmp(thread_name, "SrcRead") == 0) {
        thread_cfg->stack_size = 40 * 1024;
        thread_cfg->priority = 16;
        thread_cfg->core_id = 0;
    }
    if (strcmp(thread_name, "buffer_in") == 0) {
        thread_cfg->stack_size = 6 * 1024;
        thread_cfg->priority = 10;
        thread_cfg->core_id = 0;
    }
#endif
}

static int network_event_handler(bool connected)
{
    // Run async so that not block wifi event callback
    if (connected) {
        RUN_ASYNC(start, { start_webrtc(); });
    } else {
        RUN_ASYNC(stop, { stop_webrtc(); });
    }
    return 0;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    i2c_master_init();
    ES8388_init();
    i2c_driver_delete(I2C_NUM);
    media_lib_add_default_adapter();
    media_lib_thread_set_schedule_cb(thread_scheduler);
    init_board();
    media_sys_buildup();
    init_console();
    network_init(WIFI_SSID, WIFI_PASSWORD, network_event_handler);
    while (1) {
        media_lib_thread_sleep(2000);
        query_webrtc();
    }
}
