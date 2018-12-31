#ifndef GATT_HPP
#define GATT_HPP

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

#define ESP_APP_ID                  0x55
#define SVC_INST_ID                 0

static const char * const DEVICE_NAME = "SERVER";

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)

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

extern gatts_profile_inst profile;

extern bool ble_connected;
extern uint8_t rms_value;
extern uint16_t conn_id;

void gap_event_handler(
	esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param);

void gatts_profile_event_handler(
	esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if,
	esp_ble_gatts_cb_param_t *param);

void gatts_event_handler(
	esp_gatts_cb_event_t event,
	esp_gatt_if_t gatts_if,
	esp_ble_gatts_cb_param_t *param);

#endif
