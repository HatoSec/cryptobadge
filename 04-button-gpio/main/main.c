#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "button";

#define BUTTON_GPIO 9 /* SW2 direct GPIO, also BOOT pin */
#define LED_GPIO 8
#define LED_COUNT 16

static led_strip_handle_t strip;

static const uint8_t colors[][3] = {
    {32, 0, 0}, /* Red */
    {0, 32, 0}, /* Green */
    {0, 0, 32}, /* Blue */
    {32, 32, 0}, /* Yellow */
    {0, 32, 32}, /* Cyan */
    {32, 0, 32}, /* Magenta */
    {32, 32, 32}, /* White */
    {0, 0, 0} /* Off */
};

#define NUM_COLORS (sizeof(colors) / sizeof(colors[0]))

static void leds_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    led_strip_clear(strip);
}

static void button_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static void set_all_leds (uint8_t r, uint8_t g, uint8_t b)
{
    for (int i=0; i < LED_COUNT; i++)
        led_strip_set_pixel(strip, i, r, g, b);
    led_strip_refresh(strip);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing...");
    leds_init();
    button_init();

    int color_index = 0;
    bool was_pressed = false;

    ESP_LOGI(TAG, "Press SW2 (rightmost button) to cycle colors");
    set_all_leds(colors[0][0], colors[0][1], colors[0][2]);

    while (1) {
        bool is_pressed = (gpio_get_level(BUTTON_GPIO) == 0);

        /* Trigger only on press edge (released -> pressed) */
        if (is_pressed && !was_pressed) {
            color_index = (color_index + 1) % NUM_COLORS;
            ESP_LOGI(TAG, "Button pressed! Color index: %d", color_index);
            set_all_leds(colors[color_index][0], colors[color_index][1], colors[color_index][2]);
        }

        was_pressed = is_pressed;
        /* 20ms poll interval also serves as debounce */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
