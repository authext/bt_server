// Matching include
#include "bluetooth_server.hpp"
// C includes
#include <cstring>
// ESP includes
#include "esp_gap_ble_api.h"
#include "esp_log.h"

namespace
{
	constexpr auto TAG = "CLIENT_BLE";

	std::uint8_t service_uuid[16] =
	{
	    /* LSB <--------------------------------------------------------------------------------> MSB */
	    //first uuid, 16bit, [12],[13] is the value
	    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
	};

	constexpr std::uint16_t GATTS_SERVICE_UUID = 0x00FF;
	constexpr std::uint16_t GATTS_CHAR_UUID_RMS = 0xFF01;

	const std::uint16_t PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
	const std::uint16_t CHAR_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;
	const std::uint16_t CHAR_CLIENT_CONFIG_UUID = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
	const std::uint8_t CHAR_PROP_RN = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
	const std::uint16_t NOTIFICATIONS_ENABLED = 0x1;


	/* Full Database Description - Used to add attributes into the database */
	const esp_gatts_attr_db_t gatt_db[] =
	{
	    // Service Declaration
	    [IDX_SERVICE] =
	    {
	    	{
	    		ESP_GATT_AUTO_RSP
	    	},
			{
				ESP_UUID_LEN_16,
				(std::uint8_t *)&PRIMARY_SERVICE_UUID,
				ESP_GATT_PERM_READ,
				sizeof(GATTS_SERVICE_UUID),
				sizeof(GATTS_SERVICE_UUID),
				(std::uint8_t *)&GATTS_SERVICE_UUID
			}
	    },

	    /* Characteristic Declaration */
	    [IDX_CHAR_RMS] =
	    {
	    	{
	    		ESP_GATT_AUTO_RSP
	    	},
			{
				ESP_UUID_LEN_16,
				(std::uint8_t *)&CHAR_DECLARATION_UUID,
				ESP_GATT_PERM_READ,
				sizeof(std::uint8_t),
				sizeof(std::uint8_t),
				(std::uint8_t *)&CHAR_PROP_RN
			}
	    },

	    /* Characteristic Value */
	    [IDX_CHAR_VAL_RMS] =
	    {
	    	{
	    		ESP_GATT_AUTO_RSP
	    	},
			{
				ESP_UUID_LEN_16,
				(std::uint8_t *)&GATTS_CHAR_UUID_RMS,
				ESP_GATT_PERM_READ,
				sizeof(activator_value),
				sizeof(activator_value),
				(std::uint8_t *)&activator_value
			}
	    },

	    /* Client Characteristic Configuration Descriptor */
	    [IDX_CHAR_CFG_RMS]  =
	    {
	    	{
	    		ESP_GATT_AUTO_RSP
	    	},
			{
				ESP_UUID_LEN_16,
				(std::uint8_t *)&CHAR_CLIENT_CONFIG_UUID,
				ESP_GATT_PERM_READ,
				sizeof(NOTIFICATIONS_ENABLED),
				sizeof(NOTIFICATIONS_ENABLED),
				(std::uint8_t *)&NOTIFICATIONS_ENABLED
			}
	    },
	};

	esp_ble_adv_data_t adv_data =
	{
	    .set_scan_rsp        = false,
	    .include_name        = true,
	    .include_txpower     = true,
	    .min_interval        = 0x20,
	    .max_interval        = 0x40,
	    .appearance          = 0x00,
	    .manufacturer_len    = 0,
	    .p_manufacturer_data = NULL,
	    .service_data_len    = 0,
	    .p_service_data      = NULL,
	    .service_uuid_len    = sizeof(service_uuid),
	    .p_service_uuid      = service_uuid,
	    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
	};

	esp_ble_adv_params_t adv_params =
	{
	    .adv_int_min         = 0x20,
	    .adv_int_max         = 0x40,
	    .adv_type            = ADV_TYPE_IND,
	    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
	    .peer_addr           = {}, // Uninitialized
	    .peer_addr_type      = BLE_ADDR_TYPE_PUBLIC, // Uninitialized
	    .channel_map         = ADV_CHNL_ALL,
	    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};
}

void bluetooth_server::gap_callback(
	esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&adv_params));
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
                ESP_LOGE(TAG, "advertising start failed");
            else
                ESP_LOGI(TAG, "advertising start successfully");
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
                ESP_LOGE(TAG, "Advertising stop failed");
            else
                ESP_LOGI(TAG, "Stop adv successfully\n");
            break;

        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(
            	TAG,
				"update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                param->update_conn_params.status,
                param->update_conn_params.min_int,
                param->update_conn_params.max_int,
                param->update_conn_params.conn_int,
                param->update_conn_params.latency,
                param->update_conn_params.timeout);
            break;

        default:
            break;
    }
}

void bluetooth_server::gatts_callback(
	esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if,
	esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
    	if (param->reg.status == ESP_GATT_OK)
    	{
            m_interface = gatts_if;
    	}
    	else
		{
			ESP_LOGE(
				TAG,
				"reg app failed, app_id %04x, status %d",
				param->reg.app_id,
				param->reg.status);
			return;
		}
    }

	/* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
	const auto is_none = gatts_if == ESP_GATT_IF_NONE;
	const auto is_this = gatts_if == m_interface;

	if (!is_none && !is_this)
		return;

	switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
		ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
		ESP_ERROR_CHECK(
			esp_ble_gatts_create_attr_tab(
				gatt_db, 
				gatts_if, 
				HRS_IDX_NB, 
				0));
		break;
	}
		
	case ESP_GATTS_MTU_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
		m_ble_connected = true;
		break;

	case ESP_GATTS_CONNECT_EVT:
	{
		m_conn_id = param->connect.conn_id;
		ESP_LOGI(
			TAG,
			"ESP_GATTS_CONNECT_EVT, m_conn_id = %d",
			param->connect.conn_id);
		esp_log_buffer_hex(
			TAG,
			param->connect.remote_bda,
			6);
		esp_ble_conn_update_params_t conn_params = {};
		std::memcpy(
			conn_params.bda,
			param->connect.remote_bda,
			sizeof(esp_bd_addr_t));
		conn_params.latency = 0;
		conn_params.max_int = 0x20;    // 1.25ms
		conn_params.min_int = 0x40;    // 1.25ms
		conn_params.timeout = 10;    // 10ms
		//start sent the update connection parameters to the peer device.
		esp_ble_gap_update_conn_params(&conn_params);
		esp_ble_gap_stop_advertising();
	}
	break;

	case ESP_GATTS_DISCONNECT_EVT:
		m_ble_connected = false;
		ESP_LOGI(
			TAG,
			"ESP_GATTS_DISCONNECT_EVT, reason = %d",
			param->disconnect.reason);
		ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&adv_params));
		break;

	case ESP_GATTS_CREAT_ATTR_TAB_EVT:
	{
		if (param->add_attr_tab.status != ESP_GATT_OK)
		{
			ESP_LOGE(
				TAG,
				"create attribute table failed, error code=0x%x",
				param->add_attr_tab.status);
		}
		else if (param->add_attr_tab.num_handle != HRS_IDX_NB)
		{
			ESP_LOGE(
				TAG,
				"create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
				param->add_attr_tab.num_handle,
				HRS_IDX_NB);
		}
		else
		{
			ESP_LOGI(
				TAG,
				"create attribute table successfully, the number handle = %d\n",
				param->add_attr_tab.num_handle);
			std::memcpy(
				m_handle_table,
				param->add_attr_tab.handles,
				sizeof(m_handle_table));
			ESP_ERROR_CHECK(esp_ble_gatts_start_service(m_handle_table[IDX_SERVICE]));
		}
	}
	break;

	default:
		break;
    }
}