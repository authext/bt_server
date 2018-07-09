#ifndef A2DP_CORE_H
#define A2DP_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define BT_APP_SIG_WORK_DISPATCH          (0x01)

/**
 * @brief     handler for the dispatched work
 */
typedef void (* bt_app_cb_t) (uint16_t event, void *param);

/* message to be sent */
typedef struct
{
    uint16_t             sig;      /*!< signal to bt_app_task */
    uint16_t             event;    /*!< message event id */
    bt_app_cb_t          cb;       /*!< context switch callback */
    void                 *param;   /*!< parameter area needs to be last */
} bt_app_msg_t;

/**
 * @brief     parameter deep-copy function to be customized
 */
typedef void (* bt_app_copy_cb_t) (bt_app_msg_t *msg, void *p_dest, void *p_src);

/**
 * @brief     work dispatcher for the application task
 */
bool bt_app_work_dispatch(
	bt_app_cb_t p_cback,
	uint16_t event,
	void *p_params,
	int param_len,
	bt_app_copy_cb_t p_copy_cback);

void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

void bt_app_task_start_up(void);

void bt_app_task_shut_down(void);

extern int m_a2d_state;
extern int m_media_state;

/* A2DP global state */
enum
{
    APP_AV_STATE_IDLE,
    APP_AV_STATE_DISCOVERING,
    APP_AV_STATE_DISCOVERED,
    APP_AV_STATE_UNCONNECTED,
    APP_AV_STATE_CONNECTING,
    APP_AV_STATE_CONNECTED,
    APP_AV_STATE_DISCONNECTING,
};

/* sub states of APP_AV_STATE_CONNECTED */
enum
{
    APP_AV_MEDIA_STATE_IDLE,
    APP_AV_MEDIA_STATE_STARTING,
    APP_AV_MEDIA_STATE_STARTED,
    APP_AV_MEDIA_STATE_STOPPING,
};

typedef enum
{
    BT_APP_EVT_STACK_UP = 0,
} bt_a2d_msg;

#define BT_APP_HEART_BEAT_EVT (0xff00)

#endif
