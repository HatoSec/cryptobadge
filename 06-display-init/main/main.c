#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "display";

#define SPI_CLK_GPIO 6
#define SPI_MOSI_GPIO 7
#define SPI_MISO_GPIO (-1) /* Display is write-only */
#define LCD_CS_GPIO 1
#define LCD_DC_GPIO 0
#define LCD_RST_GPIO (-1) /* Reset is RC delay, not GPIO */

#define LCD_H_RES 320
#define LCD_V_RES 240
#define LCD_PIXEL_CLK (20*1000*1000)

#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

#define COLOR_BLACK RGB565(0, 0, 0)
#define COLOR_WHITE RGB565(255, 255, 255)
#define COLOR_RED RGB565(255, 0, 0)
#define COLOR_GREEN RGB565(0, 255, 0)
#define COLOR_BLUE RGB565(0, 0, 255)
#define COLOR_YELLOW RGB565(255, 255, 0)
#define COLOR_CYAN RGB565(0, 255, 255)
#define COLOR_MAGENTA RGB565(255, 0, 255)

static esp_lcd_panel_handle_t panel;

static void display_init(void)
{
    /* Step 1: SPI bus */
    spi_bus_config_t bus_config = {
        .sclk_io_num = SPI_CLK_GPIO,
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = SPI_MISO_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    /* Step 2: Attach display to SPI bus */
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

    /* Step 3: Create ST7789 panel driver */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    /* Step 4: Badge-specific orientation */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    ESP_LOGI(TAG, "Display initialized: %dx%d", LCD_H_RES, LCD_V_RES);
}

/* Fill entire screen with a color, one row at a time */
static void fill_screen(uint16_t color)
{
    uint16_t *row_buf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!row_buf) return;

    for (int x = 0; x < LCD_H_RES; x++) row_buf[x] = color;
    for (int y = 0; y < LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel, 0, y, LCD_H_RES, y + 1, row_buf);
    }
    free(row_buf);
}

void app_main(void)
{
    display_init();
    ESP_LOGI(TAG, "Starting display demo...");

    const struct { uint16_t color; const char *name; } colors[] = {
        {COLOR_RED, "Red"},
        {COLOR_GREEN, "Green"},
        {COLOR_BLUE, "Blue"},
        {COLOR_YELLOW, "Yellow"},
        {COLOR_CYAN, "Cyan"},
        {COLOR_MAGENTA, "Magenta"},
        {COLOR_WHITE, "White"},
        {COLOR_BLACK, "Black"},
    };

    int num = sizeof(colors) / sizeof(colors[0]);
    int i = 0;
    while (1) {
        ESP_LOGI(TAG, "Filling: %s", colors[i].name);
        fill_screen(colors[i].color);
        i = (i + 1) % num;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
