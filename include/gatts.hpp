#ifndef GATT_HPP
#define GATT_HPP

// C includes
#include <cstdint>
#include <cstddef>
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

namespace gatts
{
    constexpr auto ESP_APP_ID = 0x55;

    enum gatt_index
    {
        IDX_SERVICE,
        IDX_CHAR_RMS,
        IDX_CHAR_VAL_RMS,
        IDX_CHAR_CFG_RMS,

        HRS_IDX_NB,
    };

    extern std::uint16_t interface;

    extern bool ble_connected;
    extern std::uint8_t rms_value;
    extern std::uint16_t conn_id;
    extern std::uint16_t handle_table[HRS_IDX_NB];

    void gap_event_handler(
    	esp_gap_ble_cb_event_t event,
    	esp_ble_gap_cb_param_t *param);

    void event_handler(
    	esp_gatts_cb_event_t event,
    	esp_gatt_if_t gatts_if,
    	esp_ble_gatts_cb_param_t *param);
}

#endif
