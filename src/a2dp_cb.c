#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#include "a2dp_core.h"
#include "a2dp_cb.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tags.h"

/* a2dp event handler */
static void bt_av_hdl_a2d_evt(uint16_t event, void *p_param);
/* avrc event handler */
static void bt_av_hdl_avrc_evt(uint16_t event, void *p_param);

static uint32_t m_pkt_cnt = 0;
static esp_a2d_audio_state_t m_audio_state = ESP_A2D_AUDIO_STATE_STOPPED;
static const char *m_a2d_conn_state_str[] =
{
	"Disconnected",
	"Connecting",
	"Connected",
	"Disconnecting"
};

static const char *m_a2d_audio_state_str[] =
{
	"Suspended",
	"Stopped",
	"Started"
};

/* callback for A2DP sink */
static void a2dp_cb_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
        a2dp_core_dispatch(
        	bt_av_hdl_a2d_evt,
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
	memset(data, 0, len);

    if (++m_pkt_cnt % 100 == 0)
        ESP_LOGI(A2DP_CB_TAG, "SENT PACKETS %u", m_pkt_cnt);

    return len;
}

void bt_app_alloc_meta_buffer(esp_avrc_ct_cb_param_t *param)
{
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(param);
    uint8_t *attr_text = (uint8_t *) malloc (rc->meta_rsp.attr_length + 1);
    memcpy(attr_text, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
    attr_text[rc->meta_rsp.attr_length] = 0;

    rc->meta_rsp.attr_text = attr_text;
}

void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        bt_app_alloc_meta_buffer(param);
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
        a2dp_core_dispatch(bt_av_hdl_avrc_evt, event, param, sizeof(esp_avrc_ct_cb_param_t));
        break;
    }
    default:
        ESP_LOGE(A2DP_CB_TAG, "Invalid AVRC event: %d", event);
        break;
    }
}

static void bt_av_hdl_a2d_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        a2d = (esp_a2d_cb_param_t *)(p_param);
        uint8_t *bda = a2d->conn_stat.remote_bda;
        ESP_LOGI(A2DP_CB_TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
             m_a2d_conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT: {
        a2d = (esp_a2d_cb_param_t *)(p_param);
        ESP_LOGI(A2DP_CB_TAG, "A2DP audio state: %s", m_a2d_audio_state_str[a2d->audio_stat.state]);
        m_audio_state = a2d->audio_stat.state;
        if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
            m_pkt_cnt = 0;
        }
        break;
    }
    case ESP_A2D_AUDIO_CFG_EVT:
    	break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void bt_av_new_track()
{
    //Register notifications and request metadata
    esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_GENRE);
    esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_TRACK_CHANGE, 0);
}

void bt_av_notify_evt_handler(uint8_t event_id, uint32_t event_parameter)
{
    switch (event_id) {
    case ESP_AVRC_RN_TRACK_CHANGE:
        bt_av_new_track();
        break;
    }
}

static void bt_av_hdl_avrc_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)(p_param);
    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
        uint8_t *bda = rc->conn_stat.remote_bda;
        ESP_LOGI(A2DP_CB_TAG, "AVRC conn_state evt: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

        if (rc->conn_stat.connected) {
            bt_av_new_track();
        }
        break;
    }
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT: {
        ESP_LOGI(A2DP_CB_TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d", rc->psth_rsp.key_code, rc->psth_rsp.key_state);
        break;
    }
    case ESP_AVRC_CT_METADATA_RSP_EVT: {
        ESP_LOGI(A2DP_CB_TAG, "AVRC metadata rsp: attribute id 0x%x, %s", rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
        free(rc->meta_rsp.attr_text);
        break;
    }
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
        ESP_LOGI(A2DP_CB_TAG, "AVRC event notification: %d, param: %d", rc->change_ntf.event_id, rc->change_ntf.event_parameter);
        bt_av_notify_evt_handler(rc->change_ntf.event_id, rc->change_ntf.event_parameter);
        break;
    }
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
        ESP_LOGI(A2DP_CB_TAG, "AVRC remote features %x", rc->rmt_feats.feat_mask);
        break;
    }
    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

void a2d_cb_handle_stack_event(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CB_TAG, "%s evt %d", __func__, event);

    esp_err_t ret;

    switch (event)
    {
    case A2D_CB_EVENT_STACK_UP:
    	ESP_LOGI(A2DP_CB_TAG, "Setting up A2DP");
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

        if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CB_TAG, "Cannot set scan mode %d", ret);
        	return;
        }
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}
