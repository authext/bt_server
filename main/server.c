// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// ESP system includes
#include "esp_system.h"
// Logging includes
#include "esp_log.h"
// NVS includes
#include "nvs_flash.h"
// Bluetooth includes
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
// My includes
#include "sin_table.h"
#include "gatt.h"
#include "tag.h"

#define SAMPLES_LEN 128
static int16_t samples[SAMPLES_LEN];

static int16_t counter = 0;
static int16_t a;
static uint32_t packet_counter = 0;

void conjure_samples(void *_)
{
	for (;;)
	{
		const TickType_t start_ticks = xTaskGetTickCount();

		if (packet_counter % 200 == 0)
		{
			int r = rand();
			if (r % 4 == 0)
				a = 1;
			else if (r % 4 == 1)
				a = 2;
			else if (r % 4 == 2)
				a = 3;
			else
				a = 4;

			rms_value = a;
			printf("I have rms of %d\n", a);

			if (ble_connected)
			{
				esp_err_t ret = esp_ble_gatts_send_indicate(
					profile_tab[PROFILE_APP_IDX].gatts_if,
					conn_id,
					0x2a,
					sizeof(uint8_t),
					&rms_value,
					false);
				if (ret != ESP_OK)
				{
					ESP_LOGE(TAG, "Cannot notify");
				}
			}
		}

		for (int i = 0; i < SAMPLES_LEN; i++)
			samples[i] = a * sin_table[counter++];

		if (counter >= SIN_SIZE - 1)
			counter = 0;

		packet_counter++;

		const TickType_t end_ticks = xTaskGetTickCount();
		int ms = (end_ticks - start_ticks) * portTICK_PERIOD_MS;
		if (ms < 16)
			vTaskDelay((16 - ms) / portTICK_PERIOD_MS);
	}
}


void app_main()
{
    esp_err_t ret;

    /* Initialize NVS. */
    if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"%s init bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"%s enable bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_ble_gatts_register_callback(gatts_event_handler)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"gatts register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gap_register_callback(gap_event_handler)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"gap register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gatts_app_register(ESP_APP_ID)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"gatts app register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gatt_set_local_mtu(500)) != ESP_OK)
    {
        ESP_LOGE(
        	TAG,
			"set local  MTU failed, error code = %x",
			ret);
    }

    xTaskCreate(
    	conjure_samples,
		"conjurer",
		configMINIMAL_STACK_SIZE * 10,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL);
}
