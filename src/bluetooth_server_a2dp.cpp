// Matching include
#include "bluetooth_server.hpp"
// C includes
#include <cstring>
// ESP includes
#include "esp_a2dp_api.h"
#include "esp_log.h"

namespace
{
    constexpr auto TAG = "SERVER_A2DP";

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
}

void bluetooth_server::a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *a2d)
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
            m_a2dp_connected = true;

            ESP_LOGI(TAG, "Starting media");
            ESP_ERROR_CHECK(esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START));
        }
        else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING)
        {
            m_a2dp_connected = false;
        }
        break;

    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(
            TAG,
            "A2DP audio state: %s",
            audio_state_str[a2d->audio_stat.state]);

        if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED)
        {
            m_bytes_count = 0;
            m_packet_count = 0;
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

static constexpr std::int16_t sine_table[] = {
#include "table.txt"
};
static constexpr auto N = sizeof(sine_table)/sizeof(sine_table[0]);


std::int32_t bluetooth_server::a2dp_data_callback(std::uint8_t *data, std::int32_t len)
{
    static size_t k = 0;
    static size_t delta = round(N * 440.0/44100.0);
    static std::minstd_rand rand(321);
    static std::uniform_int_distribution<std::uint8_t> dist;

	if (len <= 0 || data == nullptr)
		return 0;

    for (std::int32_t i = 0; i < len / 2; i++)
    {
        auto value = sine_table[(k++ * delta) % N];
        data[2 * i + 0] = (value >> 0) & 0xFF;
        data[2 * i + 1] = (value >> 8) & 0xFF;
    }

	m_bytes_count += len;

    if (++m_packet_count % 256 == 0)
        ESP_LOGI(TAG, "SENT PACKETS 0x%08x (0x%08x B)", m_packet_count, m_bytes_count);

    return len;
}
