#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "gfx.h"

static const char *TAG = "draw";

#define SPI_CLK_GPIO 6
#define SPI_MOSI_GPIO 7
#define LCD_CS_GPIO 1
#define LCD_DC_GPIO 0
#define LCD_H_RES 320
#define LCD_V_RES 240
#define LCD_PIXEL_CLK (20*1000*1000)

static esp_lcd_panel_handle_t panel;
static uint16_t *framebuffer;

static void display_init(void)
{
    spi_bus_config_t bus_config = {
        .sclk_io_num = SPI_CLK_GPIO, 
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = -1, 
        .quadwp_io_num = -1, 
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO, 
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = LCD_PIXEL_CLK, 
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0, 
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1, 
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel)); 
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true)); 
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true)); 
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
}

static void flush_fb(void)
{
    for (int y=0; y < LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_H_RES, y + 1, &framebuffer[y * LCD_H_RES]);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing display..."); 
    display_init();

    framebuffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%d bytes)", LCD_H_RES * LCD_V_RES * 2);
        return;
    }
    ESP_LOGI(TAG, "Framebuffer allocated: %d bytes", LCD_H_RES * LCD_V_RES * 2);

    uint16_t bg = GFX_RGB565(0, 0, 40); 
    gfx_fill_rect(framebuffer, 0, 0, LCD_H_RES, LCD_V_RES, bg);

    /* Border */
    gfx_rect(framebuffer, 0, 0, LCD_H_RES, LCD_V_RES, GFX_RGB565(255, 255, 255));
    gfx_rect(framebuffer, 2, 2, LCD_H_RES - 4, LCD_V_RES - 4, GFX_RGB565(100, 100, 255));

    /* Title */
    gfx_string(framebuffer, 80, 20, "Cryptohack Badge", GFX_RGB565(255, 255, 0), bg);
    gfx_string(framebuffer, 72, 40, "DEF CON 34 2026", GFX_RGB565(200, 200, 200), bg);

    /* Separator */
    gfx_line(framebuffer, 10, 55, 310, 55, GFX_RGB565(100, 180, 255));

    /* Info */
    gfx_string(framebuffer, 10, 70, "Chip: ESP32-C3 (RISC-V)", GFX_RGB565(0, 255, 0), bg);
    gfx_string(framebuffer, 10, 85, "Display: ST7789 320x240", GFX_RGB565(0, 255, 0), bg);
    gfx_string(framebuffer, 10, 100, "LEDs: 16x WS2812B", GFX_RGB565(0, 255, 0), bg);
    gfx_string(framebuffer, 10, 115, "Buttons: 4 + reset", GFX_RGB565(0, 255, 0), bg);

    /* Colored rectangles */
    gfx_fill_rect(framebuffer, 10, 140, 60, 40, GFX_RGB565(255, 0, 0));
    gfx_fill_rect(framebuffer, 80, 140, 60, 40, GFX_RGB565(0, 255, 0));
    gfx_fill_rect(framebuffer, 150, 140, 60, 40, GFX_RGB565(0, 0, 255));

    /* Crossed lines */
    gfx_line(framebuffer, 230, 140, 310, 180, GFX_RGB565(255, 255, 0));
    gfx_line(framebuffer, 310, 140, 230, 180, GFX_RGB565(255, 0, 255));

    /* Horizontal lines */
    for (int i=0; i<5; i++) {
        gfx_line(framebuffer, 10, 195+i*8, 310, 195+i*8, GFX_RGB565(255, 255, 255));
    }

    /* Footer */
    gfx_string(framebuffer, 60, 220, "Hardware Hacking Tutorial", GFX_RGB565(255, 128, 0), bg);

    flush_fb();
    ESP_LOGI(TAG, "Drawing complete. Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());

    while (1) { 
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}
