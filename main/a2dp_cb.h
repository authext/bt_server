#ifndef A2DP_CB_H
#define A2DP_CB_H

#include <stdint.h>
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_gap_bt_api.h"

void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

/// callback function for A2DP source audio data stream
int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);

void a2d_app_heart_beat(void *arg);

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

#endif
