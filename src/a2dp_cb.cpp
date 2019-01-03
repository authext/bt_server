// C includes
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
// Logging includes
#include "esp_log.h"
// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// Bluetooth includes
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
// My includes
#include "a2dp_core.hpp"
#include "a2dp_cb.hpp"


static const char* const A2DP_CB_TAG = "A2DP_CB";

static void a2dp_cb_handle_a2dp_event(uint16_t event, void *param);

static uint32_t m_pkt_cnt = 0;
static esp_bd_addr_t peer_bda = { };

static const char * const conn_state_str[] =
{
	[ESP_A2D_CONNECTION_STATE_DISCONNECTED] = "Disconnected",
	[ESP_A2D_CONNECTION_STATE_CONNECTING] = "Connecting",
	[ESP_A2D_CONNECTION_STATE_CONNECTED] = "Connected",
	[ESP_A2D_CONNECTION_STATE_DISCONNECTING] = "Disconnecting",
};

static const char * const audio_state_str[] =
{
	[ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND] = "Suspended",
	[ESP_A2D_AUDIO_STATE_STOPPED] = "Stopped",
	[ESP_A2D_AUDIO_STATE_STARTED] = "Started"
};


static void a2dp_cb_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        a2dp_core::dispatch(
        	a2dp_cb_handle_a2dp_event,
			event,
			param,
			sizeof(esp_a2d_cb_param_t));
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "Invalid A2DP event: %d", event);
        break;
    }
}

static int32_t a2dp_cb_data_cb(uint8_t *data, int32_t len)
{
	static uint32_t sum_len = 0;

	if (len <= 0 || data == NULL)
		return 0;

	memset(data, rand(), len);

	sum_len += len;

    if (++m_pkt_cnt % 256 == 0)
        ESP_LOGI(A2DP_CB_TAG, "SENT PACKETS 0x%08x (0x%08x B)", m_pkt_cnt, sum_len);

    return len;
}

static void a2dp_cb_handle_a2dp_event(uint16_t event, void *param)
{
    ESP_LOGD(
    	A2DP_CB_TAG,
		"%s evt %d",
		__func__,
		event);

    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)param;

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
        ESP_LOGI(
        	A2DP_CB_TAG,
			"A2DP connection state: %s",
             conn_state_str[a2d->conn_stat.state]);

        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
        {
        	ESP_LOGI(A2DP_CB_TAG, "Connected, starting media");
        	memcpy(peer_bda, a2d->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
        	esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
        }
        else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
        {
        	ESP_LOGI(A2DP_CB_TAG, "Disconnecting");
        	memset(peer_bda, 0, sizeof(esp_bd_addr_t));
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(
        	A2DP_CB_TAG,
			"A2DP audio state: %s",
			audio_state_str[a2d->audio_stat.state]);

        if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
            m_pkt_cnt = 0;
        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    {
    	ESP_LOGI(A2DP_CB_TAG, "A2DP media event ack");
    	const bool is_cmd_start = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START;
    	const bool is_cmd_stop = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP;
    	const bool is_status_success = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;

    	if (is_cmd_start && is_status_success)
    	{
    		ESP_LOGI(A2DP_CB_TAG, "Media start successful");
    	}
    	else if (is_cmd_stop && is_status_success)
    	{
    		ESP_LOGI(A2DP_CB_TAG, "Media stop successful");
    	}
    	else
    	{
    		ESP_LOGE(
    			A2DP_CB_TAG,
				"Media command %d unsuccessful",
				a2d->media_ctrl_stat.cmd);
    	}
    	break;
    }

    default:
        ESP_LOGW(
        	A2DP_CB_TAG,
			"%s unhandled evt %d",
			__func__,
			event);
        break;
    }
}

namespace a2dp_cb
{
    void init_stack(std::uint16_t, void *)
    {
        esp_err_t ret;

    	ESP_LOGI(A2DP_CB_TAG, "Setting up A2DP");
    	esp_bt_dev_set_device_name("SERVER");
        /* initialize A2DP sink */
        if ((ret = esp_a2d_register_callback(a2dp_cb_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot register A2DP callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_source_register_data_callback(a2dp_cb_data_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot register A2DP data callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_source_init()) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot init A2DP source %d", ret);
        	return;
        }

        if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot set scan mode %d", ret);
        	return;
        }
    }
}