#include "a2dp_cb.h"
#include "a2dp_core.h"
#include "tags.h"
#include "esp_log.h"

static void bt_app_av_sm_hdlr(uint16_t event, void *param);
static void bt_app_av_state_unconnected(uint16_t event, void *param);
static void bt_app_av_state_connecting(uint16_t event, void *param);
static void bt_app_av_media_proc(uint16_t event, void *param);
static void bt_app_av_state_connected(uint16_t event, void *param);
static void bt_app_av_state_disconnecting(uint16_t event, void *param);


int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len)
{
    return len;
}

void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    bt_app_work_dispatch(
    	bt_app_av_sm_hdlr,
		event,
		param,
		sizeof(esp_a2d_cb_param_t),
		NULL);
}

void a2d_app_heart_beat(void *arg)
{
    bt_app_work_dispatch(
    	bt_app_av_sm_hdlr,
		BT_APP_HEART_BEAT_EVT,
		NULL,
		0,
		NULL);
}

static void bt_app_av_sm_hdlr(uint16_t event, void *param)
{
    ESP_LOGI(A2DP_CB_TAG, "%s state %d, evt 0x%x", __func__, m_a2d_state, event);

    switch (m_a2d_state)
    {
    case APP_AV_STATE_DISCOVERING:
    case APP_AV_STATE_DISCOVERED:
        break;

    case APP_AV_STATE_UNCONNECTED:
        bt_app_av_state_unconnected(event, param);
        break;

    case APP_AV_STATE_CONNECTING:
        bt_app_av_state_connecting(event, param);
        break;

    case APP_AV_STATE_CONNECTED:
        bt_app_av_state_connected(event, param);
        break;

    case APP_AV_STATE_DISCONNECTING:
        bt_app_av_state_disconnecting(event, param);
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s invalid state %d", __func__, m_a2d_state);
        break;
    }
}

static void bt_app_av_state_unconnected(uint16_t event, void *param)
{
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case BT_APP_HEART_BEAT_EVT:
    {
        /*uint8_t *p = peer_bda;
        ESP_LOGI(A2DP_CB_TAG, "a2dp connecting to peer: %02x:%02x:%02x:%02x:%02x:%02x",
                 p[0], p[1], p[2], p[3], p[4], p[5]);
        esp_a2d_source_connect(peer_bda);
        m_a2d_state = APP_AV_STATE_CONNECTING;
        m_connecting_intv = 0;*/
        break;
    }

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void bt_app_av_state_connecting(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = NULL;

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp connected");
            m_a2d_state =  APP_AV_STATE_CONNECTED;
            m_media_state = APP_AV_MEDIA_STATE_IDLE;
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_NONE);
        }
        else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            m_a2d_state =  APP_AV_STATE_UNCONNECTED;
        }
        break;
    }

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        break;

    case BT_APP_HEART_BEAT_EVT:
        /*if (++m_connecting_intv >= 2) {
            m_a2d_state = APP_AV_STATE_UNCONNECTED;
            m_connecting_intv = 0;
        }*/
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void bt_app_av_media_proc(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = NULL;

    switch (m_media_state)
    {
    case APP_AV_MEDIA_STATE_IDLE:
    {
        if (event == BT_APP_HEART_BEAT_EVT)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp media ready checking ...");
            esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        }
        else if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
            a2d = (esp_a2d_cb_param_t *)(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media ready, starting ...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
                m_media_state = APP_AV_MEDIA_STATE_STARTING;
            }
        }
        break;
    }

    case APP_AV_MEDIA_STATE_STARTING:
    {
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
            a2d = (esp_a2d_cb_param_t *)(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media start successfully.");
                //m_intv_cnt = 0;
                m_media_state = APP_AV_MEDIA_STATE_STARTED;
            }
            else
            {
                // not started succesfully, transfer to idle state
                ESP_LOGI(A2DP_CB_TAG, "a2dp media start failed.");
                m_media_state = APP_AV_MEDIA_STATE_IDLE;
            }
        }
        break;
    }

    case APP_AV_MEDIA_STATE_STARTED:
    {
        if (event == BT_APP_HEART_BEAT_EVT)
        {
            /*if (++m_intv_cnt >= 10)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopping...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
                m_media_state = APP_AV_MEDIA_STATE_STOPPING;
                m_intv_cnt = 0;
            }*/
        }
        break;
    }

    case APP_AV_MEDIA_STATE_STOPPING:
    {
        if (event == ESP_A2D_MEDIA_CTRL_ACK_EVT)
        {
            a2d = (esp_a2d_cb_param_t *)(param);
            if (a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP &&
                    a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopped successfully, disconnecting...");
                m_media_state = APP_AV_MEDIA_STATE_IDLE;
                //esp_a2d_source_disconnect(peer_bda);
                m_a2d_state = APP_AV_STATE_DISCONNECTING;
            }
            else
            {
                ESP_LOGI(A2DP_CB_TAG, "a2dp media stopping...");
                esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
            }
        }
        break;
    }
    }
}

static void bt_app_av_state_connected(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = NULL;

    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp disconnected");
            m_a2d_state = APP_AV_STATE_UNCONNECTED;
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        }
        break;
    }

    case ESP_A2D_AUDIO_STATE_EVT:
    {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state)
        {
            //m_pkt_cnt = 0;
        }
        break;
    }

    case ESP_A2D_AUDIO_CFG_EVT:
        // not suppposed to occur for A2DP source
        break;

    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
    {
        bt_app_av_media_proc(event, param);
        break;
    }

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

static void bt_app_av_state_disconnecting(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event)
    {
    case ESP_A2D_CONNECTION_STATE_EVT:
    {
        a2d = (esp_a2d_cb_param_t *)(param);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED)
        {
            ESP_LOGI(A2DP_CB_TAG, "a2dp disconnected");
            m_a2d_state =  APP_AV_STATE_UNCONNECTED;
            esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        }
        break;
    }

    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
    case BT_APP_HEART_BEAT_EVT:
        break;

    default:
        ESP_LOGE(A2DP_CB_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}
