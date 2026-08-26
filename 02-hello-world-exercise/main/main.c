#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"

static const char *TAG = "cryptohack";

void app_main(void)
{
	ESP_LOGI (TAG, "= Cryptohack Badge =");
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	ESP_LOGI(TAG, "Chip: %s, %d core(s), rev %d.%d", CONFIG_IDF_TARGET, chip_info.cores, chip_info.revision / 100, chip_info.revision % 100);
	ESP_LOGI(TAG, "Features:%s%s", (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " WiFi" : "", (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "");
	
uint32_t flash_size;	
if (esp_flash_get_size(NULL, &flash_size) == ESP_OK)
		ESP_LOGI(TAG, "Flash: %" PRIu32 " MB", flash_size / (1024 * 1024));
	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	ESP_LOGI(TAG, "MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[4]);
	ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());

	while (1) {
	vTaskDelay(pdMS_TO_TICKS(5000));
	}
}
