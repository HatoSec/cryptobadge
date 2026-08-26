#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "esp_log.h"
#include "ir_nec_encoder.h"

static const char *TAG = "ir";

#define IR_TX_GPIO 2
#define IR_RX_GPIO 3
#define IR_RESOLUTION_HZ 1000000 /* 1 us per tick */
#define NEC_DECODE_MARGIN 300
#define BUTTON_GPIO 9

#define NEC_LEADING_0 9000
#define NEC_LEADING_1 4500
#define NEC_ZERO_0 560
#define NEC_ZERO_1 560
#define NEC_ONE_0 560
#define NEC_ONE_1 1690

static inline bool in_range(uint32_t val, uint32_t target)
{
    return val < target + NEC_DECODE_MARGIN && val > target - NEC_DECODE_MARGIN;
}

static bool nec_decode_frame(rmt_symbol_word_t *sym, size_t count, uint16_t *address, uint16_t *command)
{
    if (count != 34) return false;
    if (!in_range(sym[0].duration0, NEC_LEADING_0) || !in_range(sym[0].duration1, NEC_LEADING_1)) return false;

    uint16_t addr = 0, cmd = 0;

    for (int i = 0; i < 16; i++) {
        rmt_symbol_word_t *s = &sym[1 + i];
        if (in_range(s->duration0, NEC_ONE_0) && in_range(s->duration1, NEC_ONE_1))
            addr |= (1 << i);
        else if (!(in_range(s->duration0, NEC_ZERO_0) && in_range(s->duration1, NEC_ZERO_1)))
            return false;
    }

    for (int i = 0; i < 16; i++) {
        rmt_symbol_word_t *s = &sym[17 + i];
        if (in_range(s->duration0, NEC_ONE_0) && in_range(s->duration1, NEC_ONE_1))
            cmd |= (1 << i);
        else if (!(in_range(s->duration0, NEC_ZERO_0) && in_range(s->duration1, NEC_ZERO_1)))
            return false;
    }

    *address = addr;
    *command = cmd;
    return true;
}

static bool rx_done_cb(rmt_channel_handle_t ch, const rmt_rx_done_event_data_t *ev, void *ctx)
{
    BaseType_t wake = pdFALSE;
    xQueueSendFromISR((QueueHandle_t)ctx, ev, &wake);
    return wake == pdTRUE;
}

void app_main(void)
{
    /* Button */
    gpio_config_t btn = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    ESP_ERROR_CHECK(gpio_config(&btn));

    /* RX */
    rmt_rx_channel_config_t rx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .gpio_num = IR_RX_GPIO
    };
    rmt_channel_handle_t rx_ch;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_cfg, &rx_ch));

    QueueHandle_t rx_q = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    rmt_rx_event_callbacks_t rx_cbs = {
        .on_recv_done = rx_done_cb
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ch, &rx_cbs, rx_q));

    rmt_receive_config_t rx_conf = {
        .signal_range_min_ns = 1250,
        .signal_range_max_ns = 12000000
    };

    /* TX */
    rmt_tx_channel_config_t tx_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .gpio_num = IR_TX_GPIO
    };
    rmt_channel_handle_t tx_ch;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &tx_ch));

    rmt_carrier_config_t carrier = {
        .duty_cycle = 0.33,
        .frequency_hz = 38000
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_ch, &carrier));

    rmt_transmit_config_t tx_conf = { .loop_count = 0 };
    ir_nec_encoder_config_t enc_cfg = { .resolution = IR_RESOLUTION_HZ };
    rmt_encoder_handle_t nec_enc;
    ESP_ERROR_CHECK(rmt_new_ir_nec_encoder(&enc_cfg, &nec_enc));

    ESP_ERROR_CHECK(rmt_enable(tx_ch));
    ESP_ERROR_CHECK(rmt_enable(rx_ch));

    rmt_symbol_word_t rx_sym[64];
    rmt_rx_done_event_data_t rx_data;
    ESP_ERROR_CHECK(rmt_receive(rx_ch, rx_sym, sizeof(rx_sym), &rx_conf));

    ESP_LOGI(TAG, "IR ready. Press SW2 to transmit. Listening...");

    bool was_pressed = false;
    uint16_t tx_counter = 0;

    while (1) {
        if (xQueueReceive(rx_q, &rx_data, pdMS_TO_TICKS(50)) == pdPASS) {
            uint16_t addr, cmd;
            if (nec_decode_frame(rx_data.received_symbols, rx_data.num_symbols, &addr, &cmd)) {
                ESP_LOGI(TAG, "RX: addr=0x%04X cmd=0x%04X", addr, cmd);
            } else {
                ESP_LOGW(TAG, "RX: decode failed (%d symbols)", (int)rx_data.num_symbols);
            }
            ESP_ERROR_CHECK(rmt_receive(rx_ch, rx_sym, sizeof(rx_sym), &rx_conf));
        }

        bool pressed = (gpio_get_level(BUTTON_GPIO) == 0);
        if (pressed && !was_pressed) {
            ir_nec_scan_code_t code = {
                .address = 0x0C00,
                .command = tx_counter++
            };
            ESP_LOGI(TAG, "TX: addr=0x%04X cmd=0x%04X", code.address, code.command);
            rmt_transmit(tx_ch, nec_enc, &code, sizeof(code), &tx_conf);
        }
        was_pressed = pressed;
    }
}
