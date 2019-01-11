// Matching include
#include "bluetooth_server.hpp"
// C++ includes
#include <chrono>
#include <thread>
// C includes
#include <cstdint>
// ESP includes
#include "esp_a2dp_api.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

using namespace std::literals;

std::uint8_t activator_value;

bluetooth_server::bluetooth_server()
	: m_bytes_count(0)
	, m_packet_count(0)
	, m_interface(ESP_GATT_IF_NONE)
	, m_conn_id(0)
	, m_ble_connected(false)
	, m_handle_table()
{
}

bluetooth_server& bluetooth_server::instance()
{
	static bluetooth_server instance;
	return instance;
}

void bluetooth_server::start()
{
	static const auto a2dp = [](
		esp_a2d_cb_event_t event,
		esp_a2d_cb_param_t *a2d)
	{
		bluetooth_server::instance().a2dp_callback(event, a2d);
	};

	static const auto a2dp_data = [](
		std::uint8_t *data,
		std::int32_t len)
	{
		return bluetooth_server::instance().a2dp_data_callback(data, len);
	};

	static const auto gap = [](
		esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param)
	{
		bluetooth_server::instance().gap_callback(event, param);
	};

	static const auto gatts = [](
		esp_gatts_cb_event_t event,
		esp_gatt_if_t iface,
		esp_ble_gatts_cb_param_t *param)
	{
		bluetooth_server::instance().gatts_callback(event, iface, param);
	};

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_dev_set_device_name("SERVER"));
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp));
    ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(a2dp_data));
    ESP_ERROR_CHECK(esp_a2d_source_init());
    ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE));

    constexpr auto ESP_APP_ID = 0x55;
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(ESP_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));

	static const auto conjure = []()
	{
		bluetooth_server::instance().conjure_rms();
	};
    std::thread(conjure).detach();
}

void bluetooth_server::conjure_rms()
{
	for (;;)
	{
		const auto start = std::chrono::steady_clock::now();

		activator_value = rand() % 10 + 1;
		printf("I have rms of %d\n", activator_value);

		if (activator_value > 2 && m_ble_connected)
		{
			ESP_ERROR_CHECK(esp_ble_gatts_send_indicate(
				m_interface,
				m_conn_id,
				m_handle_table[IDX_CHAR_VAL_RMS],
				sizeof(std::uint8_t),
				&activator_value,
				false));
		}

		const auto end = std::chrono::steady_clock::now();
		const auto diff = end - start;
		if (diff < 20s)
            std::this_thread::sleep_for(20s - diff);
	}
}
