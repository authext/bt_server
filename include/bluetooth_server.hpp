#ifndef BLUETOOTH_SERVER_HPP
#define BLUETOOTH_SERVER_HPP

// C++ includes
#include <random>
// C includes
#include <cstdint>
// ESP includes
#include "esp_a2dp_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

enum gatt_index
{
    IDX_SERVICE,
    IDX_CHAR_RMS,
    IDX_CHAR_VAL_RMS,
    IDX_CHAR_CFG_RMS,

    HRS_IDX_NB,
};

extern std::uint8_t activator_value;

class bluetooth_server
{
public:
	/* Constructors */
	bluetooth_server();
	bluetooth_server(const bluetooth_server&) = delete;
	bluetooth_server(bluetooth_server&&) = delete;

	/* Destructor */
	~bluetooth_server() = default;

	/* Operators */
	bluetooth_server& operator=(const bluetooth_server&) = delete;
	bluetooth_server& operator=(bluetooth_server&&) = delete;

	/* Static getters */
	static bluetooth_server& instance();

	/* Methods */
	void start();

private:
	/* Members */
	std::uint32_t m_bytes_count;
	std::uint32_t m_packet_count;
	std::uint16_t m_interface;
	std::uint16_t m_conn_id;
	bool m_ble_connected;
	bool m_a2dp_connected;
	std::uint16_t m_handle_table[HRS_IDX_NB];
	std::minstd_rand m_rand;
	std::uniform_int_distribution<int> m_dist;

	/* Methods */
	void a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *a2d);
	std::int32_t a2dp_data_callback(std::uint8_t *data, std::int32_t len);

	void gap_callback(
		esp_gap_ble_cb_event_t event,
		esp_ble_gap_cb_param_t *param);
	void gatts_callback(
		esp_gatts_cb_event_t event,
		esp_gatt_if_t iface,
		esp_ble_gatts_cb_param_t *param);

	void conjure_rms();
};

#endif
