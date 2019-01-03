// C includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
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
#include "gatts.hpp"
#include "a2dp_core.hpp"
#include "a2dp_cb.hpp"

static const char *const SERVER_TAG = "SERVER";

void conjure_rms(void *_)
{
	uint32_t counter = 0;

	for (;;)
	{
		const TickType_t start_ticks = xTaskGetTickCount();

		if (counter % 1500 == 0)
		{
			gatts::rms_value = rand() % 10 + 1;
			printf("I have rms of %d\n", gatts::rms_value);

			if (gatts::rms_value > 2 && gatts::ble_connected)
			{
				esp_err_t ret = esp_ble_gatts_send_indicate(
					gatts::profile.gatts_if,
					gatts::conn_id,
					0x2a,
					sizeof(uint8_t),
					&gatts::rms_value,
					false);
				if (ret != ESP_OK)
					ESP_LOGE(SERVER_TAG, "Cannot notify");
			}
		}

		counter++;

		const TickType_t end_ticks = xTaskGetTickCount();
		int ms = (end_ticks - start_ticks) * portTICK_PERIOD_MS;
		if (ms < 16)
			vTaskDelay((16 - ms) / portTICK_PERIOD_MS);
	}
}


extern "C" void app_main()
{
    esp_err_t ret;

    /* Initialize NVS. */
    if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    srand(time(NULL));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"%s enable controller failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"%s init bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"%s enable bluetooth failed: %s",
			__func__,
			esp_err_to_name(ret));
        return;
    }

    a2dp_core::start();
    a2dp_core::dispatch(
    	a2dp_cb::init_stack,
		0,
		nullptr,
		0);

    if ((ret = esp_ble_gatts_register_callback(gatts::gatts_event_handler)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"gatts register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gap_register_callback(gatts::gap_event_handler)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"gap register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gatts_app_register(gatts::ESP_APP_ID)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"gatts app register error, error code = %x",
			ret);
        return;
    }

    if ((ret = esp_ble_gatt_set_local_mtu(500)) != ESP_OK)
    {
        ESP_LOGE(
        	SERVER_TAG,
			"set local  MTU failed, error code = %x",
			ret);
    }

    xTaskCreate(
    	conjure_rms,
		"conjurer",
		configMINIMAL_STACK_SIZE * 10,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL);
}
