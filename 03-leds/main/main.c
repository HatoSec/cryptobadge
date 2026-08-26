#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "leds";

#define LED_GPIO	8	/* WS2812B data pin */
#define LED_COUNT 	16	/* Number of LEDs on the badge */
#define BRIGHTNESS	5	/* Max Brightness (0-255). Keep low to save eyes*/

static led_strip_handle_t strip;

static void leds_init(void)
{
	led_strip_config_t strip_config = {
	.strip_gpio_num = LED_GPIO,
	.max_leds = LED_COUNT,
	};
	led_strip_rmt_config_t rmt_config = {
	.resolution_hz = 10 * 1000 * 1000,	/* 10 MHz - required for WS2812B Timing */
	.flags.with_dma =  false, 		/* ESP32-C3 RMT doesnt support  DMA */
	};
	ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
	led_strip_clear(strip);
}

/*
* Convert HSV to RGB
* h: 0-359 (degrees), s: 0-255 (saturation), v: 0-255 (brightness)
*/

static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
	h %=360;
	uint8_t region = h / 60;
	uint8_t remainder = (h - (region * 60)) *255 / 60;
	uint8_t p = (v * (255 -s)) >> 8;
	uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
	uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8 ))) >> 8;

    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}


/* Demo: solid colors */
static void solid_colors (void)
{
    const uint8_t colors[][3] = { {32, 0, 0}, {0, 32, 0}, {0, 0, 32} };
    const char *names[] = {"Red", "Green", "Blue"};
    for (int c = 0; c < 3; c++) {
        ESP_LOGI(TAG, "All %s", names[c]);
        for (int i = 0; i < LED_COUNT; i++) {
            led_strip_set_pixel(strip, i, colors[c][0], colors[c][1], colors[c][2]);
        }
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* Animation: rainbow chase - each LED shows a different hue, rotating over time */
static void rainbow_chase(int duration_ms)
{
    int offset = 0;
    int frames = duration_ms / 30;
    for (int frame = 0; frame < frames; frame++) {
        for (int i = 0; i < LED_COUNT; i++) {
            uint16_t hue = (i * 360 / LED_COUNT + offset) % 360;
            uint8_t r, g, b;
            hsv_to_rgb(hue, 255, BRIGHTNESS, &r, &g, &b);
            led_strip_set_pixel(strip, i, r, g, b);
        }
        led_strip_refresh(strip);
        offset = (offset + 5) % 360;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

/* Animation: Larson scanner - a bright LED bounces with a fading trail */
static void larson_scanner (int duration_ms)
{
    ESP_LOGI(TAG, "Larson scanner");
    int pos = 0;
    int direction = 1;
    int frames = duration_ms / 50;
    for (int frame = 0; frame < frames; frame++) {
        for (int i = 0; i < LED_COUNT; i++) {
            int distance = abs(i - pos);
            uint8_t brightness;
            if (distance == 0) brightness = BRIGHTNESS;
            else if (distance == 1) brightness = BRIGHTNESS / 3;
            else if (distance == 2) brightness = BRIGHTNESS / 8;
            else brightness = 0;
            led_strip_set_pixel(strip, i, brightness, 0, 0);
        }
        led_strip_refresh(strip);
        pos += direction;
        if (pos >= LED_COUNT - 1 || pos <= 0) direction = -direction;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* Animation: breathing - all LEDs pulse using a sine wave */
static void breathing (int duration_ms)
{
    ESP_LOGI(TAG, "Breathing");
    int frames = duration_ms / 20;
    for (int frame = 0; frame < frames; frame++) {
        float phase = (float)frame / 80.0f;
        float val = (sinf(phase * 2.0f * 3.14159f) + 1.0f) / 2.0f;
        uint8_t brightness = (uint8_t)(val * BRIGHTNESS);
        for (int i = 0; i < LED_COUNT; i++)
            led_strip_set_pixel(strip, i, 0, brightness / 2, brightness);
        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing LEDs...");
    leds_init();
    solid_colors();
    while (1) {
        rainbow_chase(10000);
        larson_scanner(10000);
        breathing(10000);
    }
}
