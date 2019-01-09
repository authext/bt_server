// Matching include
#include "a2dp_cb.hpp"
// C includes
#include <cstdint>
#include <cstring>
// Logging includes
#include "esp_log.h"
// Bluetooth includes
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"

namespace
{
    constexpr auto TAG = "A2DP_CB";

    std::uint32_t sum_len = 0;
    std::uint32_t packet_count = 0;

    const char * const conn_state_str[] =
    {
    	[ESP_A2D_CONNECTION_STATE_DISCONNECTED] = "Disconnected",
    	[ESP_A2D_CONNECTION_STATE_CONNECTING] = "Connecting",
    	[ESP_A2D_CONNECTION_STATE_CONNECTED] = "Connected",
    	[ESP_A2D_CONNECTION_STATE_DISCONNECTING] = "Disconnecting",
    };

    const char * const audio_state_str[] =
    {
    	[ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND] = "Suspended",
    	[ESP_A2D_AUDIO_STATE_STOPPED] = "Stopped",
    	[ESP_A2D_AUDIO_STATE_STARTED] = "Started"
    };

    void callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *a2d)
    {
        ESP_LOGD(
            TAG,
            "%s evt %d",
            __func__,
            event);

        switch (event)
        {
        case ESP_A2D_CONNECTION_STATE_EVT:
            ESP_LOGI(
                TAG,
                "A2DP connection state: %s",
                 conn_state_str[a2d->conn_stat.state]);

            if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED)
            {
                ESP_LOGI(TAG, "Connected, starting media");
                //esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
            }
            else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
            {
                ESP_LOGI(TAG, "Disconnecting");
            }
            break;

        case ESP_A2D_AUDIO_STATE_EVT:
            ESP_LOGI(
                TAG,
                "A2DP audio state: %s",
                audio_state_str[a2d->audio_stat.state]);

            if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
            {
                sum_len = 0;
                packet_count = 0;
            }
            break;

        case ESP_A2D_MEDIA_CTRL_ACK_EVT:
        {
            ESP_LOGI(TAG, "A2DP media event ack");
            const auto is_cmd_start = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_START;
            const auto is_cmd_stop = a2d->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_STOP;
            const auto is_status_success = a2d->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;

            if (is_cmd_start && is_status_success)
            {
                ESP_LOGI(TAG, "Media start successful");
            }
            else if (is_cmd_stop && is_status_success)
            {
                ESP_LOGI(TAG, "Media stop successful");
            }
            else
            {
                ESP_LOGE(
                    TAG,
                    "Media command %d unsuccessful",
                    a2d->media_ctrl_stat.cmd);
            }
            break;
        }

        default:
            ESP_LOGW(
                TAG,
                "%s unhandled evt %d",
                __func__,
                event);
            break;
        }
    }

    std::int32_t data_callback(std::uint8_t *data, std::int32_t len)
    {
    	if (len <= 0 || data == nullptr)
    		return 0;

    	std::memset(data, rand(), len);

    	sum_len += len;

        if (++packet_count % 256 == 0)
            ESP_LOGI(TAG, "SENT PACKETS 0x%08x (0x%08x B)", packet_count, sum_len);

        return len;
    }
}

namespace a2dp_cb
{
    void init_stack()
    {
    	ESP_LOGI(TAG, "Setting up A2DP");

    	ESP_ERROR_CHECK(esp_bt_dev_set_device_name("SERVER"));
        ESP_ERROR_CHECK(esp_a2d_register_callback(callback));
        ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(data_callback));
        ESP_ERROR_CHECK(esp_a2d_source_init());
        ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE));
    }
}