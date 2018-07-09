#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/xtensa_api.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "a2dp_core.h"
#include "tags.h"
#include "esp_a2dp_api.h"
#include "esp_gap_bt_api.h"

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
/// callback function for A2DP source audio data stream
int32_t bt_app_a2d_data_cb(uint8_t *data, int32_t len);
void a2d_app_heart_beat(void *arg);

int m_a2d_state = APP_AV_STATE_IDLE;
int m_media_state = APP_AV_MEDIA_STATE_IDLE;

TimerHandle_t tmr;

static void bt_app_task_handler(void *arg);
static bool bt_app_send_msg(bt_app_msg_t *msg);
static void bt_app_work_dispatched(bt_app_msg_t *msg);

static xQueueHandle bt_app_task_queue = NULL;
static xTaskHandle bt_app_task_handle = NULL;

bool bt_app_work_dispatch(
	bt_app_cb_t p_cback,
	uint16_t event,
	void *p_params,
	int param_len,
	bt_app_copy_cb_t p_copy_cback)
{
    ESP_LOGD(A2DP_CORE_TAG, "%s event 0x%x, param len %d", __func__, event, param_len);

    bt_app_msg_t msg =
    {
    	.sig = BT_APP_SIG_WORK_DISPATCH,
		.event = event,
		.cb = p_cback
    };

    if (param_len == 0)
    {
        return bt_app_send_msg(&msg);
    }
    else if (p_params && param_len > 0)
    {
        if ((msg.param = malloc(param_len)) != NULL)
        {
            memcpy(msg.param, p_params, param_len);
            /* check if caller has provided a copy callback to do the deep copy */
            if (p_copy_cback)
                p_copy_cback(&msg, msg.param, p_params);

            return bt_app_send_msg(&msg);
        }
    }

    return false;
}

static bool bt_app_send_msg(bt_app_msg_t *msg)
{
    if (msg == NULL)
        return false;

    if (xQueueSend(bt_app_task_queue, msg, 10 / portTICK_RATE_MS) != pdTRUE)
    {
        ESP_LOGE(A2DP_CORE_TAG, "%s xQueue send failed", __func__);
        return false;
    }

    return true;
}

static void bt_app_work_dispatched(bt_app_msg_t *msg)
{
    if (msg->cb)
        msg->cb(msg->event, msg->param);
}

static void bt_app_task_handler(void *arg)
{
    for (;;)
    {
    	bt_app_msg_t msg;

        if (pdTRUE == xQueueReceive(bt_app_task_queue, &msg, (portTickType)portMAX_DELAY))
        {
            ESP_LOGD(A2DP_CORE_TAG, "%s, sig 0x%x, 0x%x", __func__, msg.sig, msg.event);
            switch (msg.sig)
            {
            case BT_APP_SIG_WORK_DISPATCH:
                bt_app_work_dispatched(&msg);
                break;

            default:
                ESP_LOGW(A2DP_CORE_TAG, "%s, unhandled sig: %d", __func__, msg.sig);
                break;
            }

            if (msg.param)
                free(msg.param);
        }
    }
}

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BT_GAP_RMT_SRVCS_EVT:
    case ESP_BT_GAP_RMT_SRVC_REC_EVT:
        break;

    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGI(A2DP_CORE_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            esp_log_buffer_hex(A2DP_CORE_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        }
        else
        {
            ESP_LOGI(A2DP_CORE_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;

    default:
        ESP_LOGI(A2DP_CORE_TAG, "event: %d", event);
        break;
    }
}

void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(A2DP_CORE_TAG, "%s evt %d", __func__, event);

    int tmr_id;
    esp_err_t ret;

    switch (event)
    {
    case BT_APP_EVT_STACK_UP:
    	ESP_LOGI(A2DP_CORE_TAG, "Setting up a2dp stack");

        /* register GAP callback function */
        if ((ret = esp_bt_gap_register_callback(bt_app_gap_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CORE_TAG, "Cannot register A2DP GAP callback %d", ret);
        	return;
        }

        /* initialize A2DP source */
        if ((ret = esp_a2d_register_callback(&bt_app_a2d_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CORE_TAG, "Cannot register A2DP callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_source_register_data_callback(bt_app_a2d_data_cb)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CORE_TAG, "Cannot register A2DP data callback %d", ret);
        	return;
        }

        if ((ret = esp_a2d_source_init()) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CORE_TAG, "Cannot init A2DP %d", ret);
        	return;
        }

        /* set discoverable and connectable mode */
        if ((ret = esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE)) != ESP_OK)
        {
        	ESP_LOGE(A2DP_CORE_TAG, "Cannot set scan mode %d", ret);
        	return;
        }

        /* create and start heart beat timer */
        tmr_id = 0;
        tmr = xTimerCreate(
        	"connTmr",
			(10000 / portTICK_RATE_MS),
            pdTRUE,
			(void *)tmr_id,
			a2d_app_heart_beat);

        //xTimerStart(tmr, portMAX_DELAY);
        break;

    default:
        ESP_LOGE(A2DP_CORE_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}

void bt_app_task_start_up(void)
{
    bt_app_task_queue = xQueueCreate(10, sizeof(bt_app_msg_t));
    xTaskCreate(
    	bt_app_task_handler,
		"BtAppT",
		2048,
		NULL,
		configMAX_PRIORITIES - 3,
		&bt_app_task_handle);
    return;
}

void bt_app_task_shut_down(void)
{
    if (bt_app_task_handle)
    {
        vTaskDelete(bt_app_task_handle);
        bt_app_task_handle = NULL;
    }

    if (bt_app_task_queue)
    {
        vQueueDelete(bt_app_task_queue);
        bt_app_task_queue = NULL;
    }
}
