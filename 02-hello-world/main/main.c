#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
	printf("Cryptohack Badge is alive!\n");

	int i = 0;
	while (1) {
		printf("tick %d\n", i++);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
