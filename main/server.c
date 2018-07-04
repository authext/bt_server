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


static const char *TAG = "GATT_SERVER";


typedef enum _gatt_index
{
    IDX_SERVICE,
    IDX_CHAR_RMS,
    IDX_CHAR_VAL_RMS,
    IDX_CHAR_CFG_RMS,

    HRS_IDX_NB,
} gatt_index;

#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SAMPLE_DEVICE_NAME          "GATT_SERVER"
#define SVC_INST_ID                 0

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

static uint8_t adv_config_done       = 0;

uint16_t handle_table[HRS_IDX_NB];


static uint8_t service_uuid[16] =
{
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
};

/* The length of adv data must be less than 31 bytes */
static esp_ble_adv_data_t adv_data =
{
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x20,
    .max_interval        = 0x40,
    .appearance          = 0x00,
    .manufacturer_len    = 0,    //TEST_MANUFACTURER_DATA_LEN,
    .p_manufacturer_data = NULL, //test_manufacturer,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(service_uuid),
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// scan response data
static esp_ble_adv_data_t scan_rsp_data =
{
    .set_scan_rsp        = true,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x20,
    .max_interval        = 0x40,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 16,
    .p_service_uuid      = service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params =
{
    .adv_int_min         = 0x20,
    .adv_int_max         = 0x40,
    .adv_type            = ADV_TYPE_IND,
    .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
    .channel_map         = ADV_CHNL_ALL,
    .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_profile_inst
{
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

static void gatts_profile_event_handler(
		esp_gatts_cb_event_t event,
		esp_gatt_if_t gatts_if,
		esp_ble_gatts_cb_param_t *param);

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] =
{
    [PROFILE_APP_IDX] =
    {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/* Service */
static const uint16_t GATTS_SERVICE_UUID = 0x00FF;
static const uint16_t GATTS_CHAR_UUID_RMS = 0xFF01;

static const uint16_t PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t CHAR_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t CHAR_CLIENT_CONFIG_UUID = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t CHAR_PROP_READ = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint16_t NOTIFICATIONS_ENABLED = 0x0;
static uint8_t rms_value = 0;


/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t gatt_db[HRS_IDX_NB] =
{
    // Service Declaration
    [IDX_SERVICE] =
    {
    	{
    		ESP_GATT_AUTO_RSP
    	},
		{
			ESP_UUID_LEN_16,
			(uint8_t *)&PRIMARY_SERVICE_UUID,
			ESP_GATT_PERM_READ,
			sizeof(GATTS_SERVICE_UUID),
			sizeof(GATTS_SERVICE_UUID),
			(uint8_t *)&GATTS_SERVICE_UUID
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
			(uint8_t *)&CHAR_DECLARATION_UUID,
			ESP_GATT_PERM_READ,
			sizeof(uint8_t),
			sizeof(uint8_t),
			(uint8_t *)&CHAR_PROP_READ
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
			(uint8_t *)&GATTS_CHAR_UUID_RMS,
			ESP_GATT_PERM_READ,
			sizeof(rms_value),
			sizeof(rms_value),
			(uint8_t *)&rms_value
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
			(uint8_t *)&CHAR_CLIENT_CONFIG_UUID,
			ESP_GATT_PERM_READ,
			sizeof(NOTIFICATIONS_ENABLED),
			sizeof(NOTIFICATIONS_ENABLED),
			(uint8_t *)&NOTIFICATIONS_ENABLED
		}
    },
};

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


static void gap_event_handler(
	esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0)
                esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0)
                esp_ble_gap_start_advertising(&adv_params);
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

static void gatts_profile_event_handler(
	esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if,
	esp_ble_gatts_cb_param_t *param)
{
	esp_err_t ret;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
		if ((ret = esp_ble_gap_set_device_name(SAMPLE_DEVICE_NAME)) != ESP_OK)
			ESP_LOGE(TAG, "set device name failed, error code = %x", ret);

		//config adv data
		if ((ret = esp_ble_gap_config_adv_data(&adv_data)) != ESP_OK)
			ESP_LOGE(TAG, "config adv data failed, error code = %x", ret);
		adv_config_done |= ADV_CONFIG_FLAG;

		//config scan response data
		if ((ret = esp_ble_gap_config_adv_data(&scan_rsp_data)) != ESP_OK)
			ESP_LOGE(TAG, "config scan response data failed, error code = %x", ret);
		adv_config_done |= SCAN_RSP_CONFIG_FLAG;

		if ((ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_IDX_NB, SVC_INST_ID)) != ESP_OK)
			ESP_LOGE(TAG, "create attr table failed, error code = %x", ret);
		break;

	case ESP_GATTS_READ_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_READ_EVT");
		break;

	case ESP_GATTS_MTU_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
		break;

	case ESP_GATTS_CONF_EVT:
		ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT, status = %d", param->conf.status);
		break;

	case ESP_GATTS_START_EVT:
		ESP_LOGI(TAG, "SERVICE_START_EVT, status %d, service_handle %d", param->start.status, param->start.service_handle);
		break;

	case ESP_GATTS_CONNECT_EVT:
		ESP_LOGI(
			TAG,
			"ESP_GATTS_CONNECT_EVT, conn_id = %d",
			param->connect.conn_id);
		esp_log_buffer_hex(
			TAG,
			param->connect.remote_bda,
			6);
		esp_ble_conn_update_params_t conn_params = {0};
		memcpy(
			conn_params.bda,
			param->connect.remote_bda,
			sizeof(esp_bd_addr_t));
		conn_params.latency = 0;
		conn_params.max_int = 0x20;    // 1.25ms
		conn_params.min_int = 0x40;    // 1.25ms
		conn_params.timeout = 10;    // 10ms
		//start sent the update connection parameters to the peer device.
		esp_ble_gap_update_conn_params(&conn_params);
		break;

	case ESP_GATTS_DISCONNECT_EVT:
		ESP_LOGI(
			TAG,
			"ESP_GATTS_DISCONNECT_EVT, reason = %d",
			param->disconnect.reason);
		esp_ble_gap_start_advertising(&adv_params);
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
			memcpy(
				handle_table,
				param->add_attr_tab.handles,
				sizeof(handle_table));
			esp_ble_gatts_start_service(handle_table[IDX_SERVICE]);
		}
	}
	break;

	default:
		break;
    }
}


static void gatts_event_handler(
	esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if,
	esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT)
    {
    	if (param->reg.status == ESP_GATT_OK)
    	{
            heart_rate_profile_tab[PROFILE_APP_IDX].gatts_if = gatts_if;
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

	for (int idx = 0; idx < PROFILE_NUM; idx++)
	{
		/* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
		const bool is_none = gatts_if == ESP_GATT_IF_NONE;
		const bool is_this = gatts_if == heart_rate_profile_tab[idx].gatts_if;

		if ((is_none || is_this) && heart_rate_profile_tab[idx].gatts_cb)
			heart_rate_profile_tab[idx].gatts_cb(event, gatts_if, param);
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
		configMINIMAL_STACK_SIZE * 2,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL);
}
