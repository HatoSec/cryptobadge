#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "eeprom";

#define SPI_CLK_GPIO 0
#define SPI_MOSI_GPIO 7
#define SPI_MISO_GPIO 5 /* Need MISO for EEPROM reads */

#define EEPROM_CS_GPIO 4

#define CMD_WREN 0x06
#define CMD_RDSR 0x05
#define CMD_WRITE 0x02
#define CMD_READ 0x03
#define SR_WIP 0x01

static spi_device_handle_t eeprom_dev;

static void spi_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = SPI_CLK_GPIO,
        .mosi_io_num = SPI_MOSI_GPIO,
        .miso_io_num = SPI_MISO_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 5 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = EEPROM_CS_GPIO,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &eeprom_dev));
}

static void eeprom_wait_ready(void)
{
    uint8_t tx[2] = { CMD_RDSR, 0 }, rx[2] = { 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx
    };
    while (1) {
        spi_device_transmit(eeprom_dev, &t);
        if (!(rx[1] & SR_WIP)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static esp_err_t eeprom_write(uint16_t addr, const uint8_t *data, size_t len)
{
    if (len == 0 || len > 32) return ESP_ERR_INVALID_SIZE;

    /* Write Enable */
    uint8_t wren = CMD_WREN;
    spi_transaction_t tw = {
        .length = 8,
        .tx_buffer = &wren
    };
    spi_device_transmit(eeprom_dev, &tw);

    /* Write data */
    uint8_t tx[35];
    tx[0] = CMD_WRITE;
    tx[1] = addr >> 8;
    tx[2] = addr & 0xFF;
    memcpy(&tx[3], data, len);

    spi_transaction_t t = {
        .length = (3 + len) * 8,
        .tx_buffer = tx
    };

    esp_err_t ret = spi_device_transmit(eeprom_dev, &t);
    if (ret == ESP_OK) eeprom_wait_ready();
    return ret;
}

static esp_err_t eeprom_read(uint16_t addr, uint8_t *data, size_t len)
{
    uint8_t tx[67] = {0}, rx[67] = {0};
    tx[0] = CMD_READ;
    tx[1] = addr >> 8;
    tx[2] = addr & 0xFF;

    spi_transaction_t t = {
        .length = (3 + len) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx
    };

    esp_err_t ret = spi_device_transmit(eeprom_dev, &t);
    if (ret == ESP_OK) memcpy(data, &rx[3], len);
    return ret;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing SPI + EEPROM...");
    spi_init();

    /* Diagnostic: read status register to check EEPROM is responding */
    {
        uint8_t tx[2] = { CMD_RDSR, 0 }, rx[2] = { 0 };
        spi_transaction_t t = {
            .length = 16,
            .tx_buffer = tx,
            .rx_buffer = rx
        };
        ESP_ERROR_CHECK(spi_device_transmit(eeprom_dev, &t));
        ESP_LOGI(TAG, "RDSR = 0x%02x (expect non-zero if EEPROM present)", rx[1]);
    }

    /* Read current data */
    uint8_t existing[32] = {0};
    eeprom_read(0x0000, existing, 32);
    ESP_LOGI(TAG, "Current data: \"%.*s\"", 32, existing);

    /* Write a string */
    const char *msg = "Cryptohack Badge!";
    ESP_LOGI(TAG, "Writing: \"%s\"", msg);
    eeprom_write(0x0000, (const uint8_t *)msg, strlen(msg) + 1);

    /* Read back and verify */
    uint8_t readback[32] = {0};
    eeprom_read(0x0000, readback, strlen(msg) + 1);

    ESP_LOGI(TAG, "Readback (%d bytes):", (int)(strlen(msg) + 1));
    for (int i = 0; i < (int)(strlen(msg) + 1); i++) {
        ESP_LOGI(TAG, "  [%02d] 0x%02x '%c'", i, readback[i],
                 (readback[i] >= 0x20 && readback[i] < 0x7f) ? readback[i] : '.');
    }

    if (memcmp(msg, readback, strlen(msg) + 1) == 0)
        ESP_LOGI(TAG, "Verified!");
    else
        ESP_LOGE(TAG, "Verification FAILED!");

    /* Boot counter at address 0x0100 */
    uint32_t counter = 0;
    eeprom_read(0x0100, (uint8_t *)&counter, sizeof(counter));
    ESP_LOGI(TAG, "Boot counter: %" PRIu32, counter);
    counter++;
    eeprom_write(0x0100, (uint8_t *)&counter, sizeof(counter));
    ESP_LOGI(TAG, "Updated to: %" PRIu32, counter);

    ESP_LOGI(TAG, "Data persists across reboots -- try resetting!");
    while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
}
