#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "i2c-btn";

/* I2C bus */
#define I2C_SDA_GPIO 20
#define I2C_SCL_GPIO 21
#define I2C_FREQ_HZ 100000
#define I2C_TIMEOUT_MS 1000

/* PCF8574 I/O expander */
#define PCF8574_ADDR 0x20
#define BTN_SW3_BIT 1 /* P1 left button */
#define BTN_SW4_BIT 2 /* P2 mid-left button */
#define BTN_SW1_BIT 3 /* P3 mid-right button */

/* Direct GPIO button */
#define BTN_SW2_GPIO 9

/* LEDs */
#define LED_GPIO 8
#define LED_COUNT 16

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t pcf8574_dev;
static led_strip_handle_t strip;

static void i2c_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF8574_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_config, &pcf8574_dev));
}

static void i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 1; addr < 0x78; addr++) {
        if (i2c_master_probe(i2c_bus, addr, I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGI(TAG, " Found device at 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "Scan complete. %d device(s) found.", found);
}

static void leds_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    led_strip_clear(strip);
}

static void button_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BTN_SW2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

typedef struct {
    bool sw1, sw2, sw3, sw4;
} button_state_t;

static button_state_t read_buttons(void)
{
    button_state_t state = {0};
    uint8_t data = 0xFF;
    if (i2c_master_receive(pcf8574_dev, &data, 1, I2C_TIMEOUT_MS) == ESP_OK) {
        state.sw3 = !((data >> BTN_SW3_BIT) & 1);
        state.sw4 = !((data >> BTN_SW4_BIT) & 1);
        state.sw1 = !((data >> BTN_SW1_BIT) & 1);
    }
    state.sw2 = (gpio_get_level(BTN_SW2_GPIO) == 0);
    return state;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing...");
    i2c_init();
    leds_init();
    button_gpio_init();
    i2c_scan();

    ESP_LOGI(TAG, "Button layout (left to right): SW3, SW4, SW1, SW2");
    button_state_t prev = {0};

    while (1) {
        button_state_t btn = read_buttons();

        if (btn.sw1 != prev.sw1 || btn.sw2 != prev.sw2 || btn.sw3 != prev.sw3 || btn.sw4 != prev.sw4) {
            ESP_LOGI(TAG, "SW3:%d SW4:%d SW1:%d SW2:%d", btn.sw3, btn.sw4, btn.sw1, btn.sw2);
            
            for (int i = 0; i < LED_COUNT; i++) {
                led_strip_set_pixel(strip, i, 0, 0, 0);
            }

            if (btn.sw3) for (int i = 0; i < 4; i++) led_strip_set_pixel(strip, i, 32, 0, 0);
            if (btn.sw4) for (int i = 4; i < 8; i++) led_strip_set_pixel(strip, i, 0, 32, 0);
            if (btn.sw1) for (int i = 8; i < 12; i++) led_strip_set_pixel(strip, i, 0, 0, 32);
            if (btn.sw2) for (int i = 12; i < 16; i++) led_strip_set_pixel(strip, i, 32, 32, 32);
            
            led_strip_refresh(strip);
            prev = btn;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
