#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "nfc";

#define I2C_SDA 20
#define I2C_SCL 21
#define NTAG_ADDR 0x55
#define TIMEOUT (1000 / portTICK_PERIOD_MS)

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t ntag;

static void i2c_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = NTAG_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &ntag));
}

/* Read a 16-byte block */
static esp_err_t ntag_read_block(uint8_t block, uint8_t *data)
{
    return i2c_master_transmit_receive(ntag, &block, 1, data, 16, TIMEOUT);
}

/* Write a 16-byte block */
static esp_err_t ntag_write_block(uint8_t block, const uint8_t *data)
{
    uint8_t buf[17];
    buf[0] = block;
    memcpy(&buf[1], data, 16);
    esp_err_t ret = i2c_master_transmit(ntag, buf, 17, TIMEOUT);
    if (ret == ESP_OK) vTaskDelay(pdMS_TO_TICKS(5)); /* Write cycle time */
    return ret;
}

/* Dump all readable blocks */
static void ntag_dump(void)
{
    ESP_LOGI(TAG, "--- NTAG Memory Dump ---");
    for (int b = 0; b <= 0x3A; b++) {
        uint8_t data[16];
        if (ntag_read_block(b, data) == ESP_OK) {
            ESP_LOGI(TAG, "[%02X] %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     b, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                     data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
        }
    }
}

/* Write an NDEF text record to the NTAG. This makes the tag readable by any NFC phone. */
static void ntag_write_ndef_text(const char *text)
{
    int text_len = strlen(text);
    int lang_len = 2; /* "en" */
    int payload_len = 1 + lang_len + text_len; /* status byte + lang + text */
    int ndef_len = 1 + 1 + 1 + 1 + payload_len; /* header(1) + type_len (1) + payload_len(1) + type(1) + payload */
    int total = 2 + ndef_len + 1; /* TLV tag(1) + TLV len (1) + ndef + terminator (1) */

    if (total > 888) {
        ESP_LOGE(TAG, "Text too long (max ~115 chars)");
        return;
    }

    /* Build the NDEF message in a buffer */
    uint8_t msg[128] = {0};
    int pos = 0;

    /* NDEF message TLV */
    msg[pos++] = 0x03;
    msg[pos++] = (uint8_t)ndef_len;

    /* NDEF record header: MB=1, ME=1, CF=0, SR=1, IL=0, TNF=001 (well-known) */
    msg[pos++] = 0xD1;
    msg[pos++] = 0x01; /* Type length = 1 */
    msg[pos++] = (uint8_t)payload_len; /* Payload length */
    msg[pos++] = 0x54; /* Type = 'T' (text) */

    /* Payload: status byte + language + text */
    msg[pos++] = (uint8_t)lang_len; /* Status: UTF-8, lang length = 2 */
    msg[pos++] = 'e';
    msg[pos++] = 'n';
    memcpy(&msg[pos], text, text_len);
    pos += text_len;

    /* Terminator TLV */
    msg[pos++] = 0xFE;

    /* Write CC to block 0x00 (only bytes 12-15, preserve UID bytes 0-11) */
    uint8_t block0[16];
    ntag_read_block(0x00, block0);
    block0[12] = 0xE1; /* Magic number marks tag as NDEF capable */
    block0[13] = 0x10; /* Version 1.0 */
    block0[14] = 0x6D; /* Size = 109*8 = 872 bytes */
    block0[15] = 0x00; /* Read/write access */
    ntag_write_block(0x00, block0);

    /* Write NDEF data starting at block 0x01 */
    int block = 0x01;
    int offset = 0;
    while (offset < pos) {
        uint8_t blk_data[16] = {0};
        int chunk = pos - offset;
        if (chunk > 16) chunk = 16;
        memcpy(blk_data, &msg[offset], chunk);
        ntag_write_block(block, blk_data);
        block++;
        offset += 16;
    }
    ESP_LOGI(TAG, "Wrote NDEF text record: \"%s\" (%d bytes, %d blocks)", text, pos, block - 1);
}

void app_main(void)
{
    i2c_init();
    ESP_LOGI(TAG, "Initializing I2C...");

    /* Read UID */
    uint8_t block0[16];
    ntag_read_block(0x00, block0);
    ESP_LOGI(TAG, "NTAG UID: %02X:%02X:%02X:%02X:%02X:%02X:%02X",
             block0[0], block0[1], block0[2], block0[4], block0[5], block0[6], block0[7]);

    /* Dump current memory */
    ntag_dump();

    /* Write an NDEF text record */
    ESP_LOGI(TAG, "\nWriting NDEF text record...");
    ntag_write_ndef_text("Cryptohack Badge DEF CON 34");

    /* Dump again to see the written data */
    ESP_LOGI(TAG, "\nAfter writing:");
    ntag_dump();

    ESP_LOGI(TAG, "\nDone! Tap the badge with your phone to read the NFC tag.");
    while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
}
