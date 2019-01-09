// C++ includes
#include <chrono>
#include <thread>
// C includes
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <ctime>
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
#include "a2dp_cb.hpp"

using namespace std::literals;

namespace
{
	constexpr auto TAG = "SERVER";
}

void conjure_rms()
{
	std::uint32_t counter = 0;

	for (;;)
	{
		const auto start = std::chrono::steady_clock::now();

		if (counter % 1500 == 0)
		{
			gatts::rms_value = rand() % 10 + 1;
			printf("I have rms of %d\n", gatts::rms_value);

			if (gatts::rms_value > 2 && gatts::ble_connected)
			{
				const auto ret = esp_ble_gatts_send_indicate(
					gatts::interface,
					gatts::conn_id,
					0x2a,
					sizeof(std::uint8_t),
					&gatts::rms_value,
					false);
				if (ret != ESP_OK)
					ESP_LOGE(TAG, "Cannot notify");
			}
		}

		counter++;

		const auto end = std::chrono::steady_clock::now();
		const auto diff = end - start;
		if (diff < 16ms)
            std::this_thread::sleep_for(16ms - diff);
	}
}


extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    srand(time(NULL));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    a2dp_cb::init_stack();

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts::event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gatts::gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(gatts::ESP_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));

    std::thread(conjure_rms).detach();
}
