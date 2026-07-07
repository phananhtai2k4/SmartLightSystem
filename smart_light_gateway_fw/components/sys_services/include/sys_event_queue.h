// components/sys_services/include/sys_event_queue.h

#ifndef SYS_EVENT_QUEUE_H
#define SYS_EVENT_QUEUE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include <time.h>
#include "esp_ble_mesh_defs.h"
#include "ble_mesh_core.h"

typedef enum {
    EVENT_SYSTEM_STARTUP,
    EVENT_NETWORK_CONNECTED,
    EVENT_NETWORK_DISCONNECTED,
    EVENT_MESH_PROV_COMPLETE,
    EVENT_MESH_CONFIG_RECEIVED,
    EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED,
    EVENT_MESH_GENERIC_ONOFF_SET_ACK_RECEIVED,
    EVENT_MESH_GENERIC_ONOFF_GET_ACK_RECEIVED,
    EVENT_MESH_VENDOR_PUBLISH_RECEIVED,
    EVENT_BUTTON_PRESSED,
    EVENT_BUTTON_RELEASED,
    EVENT_BUTTON_HOLD_3S,
    EVENT_BUTTON_HOLD_5S,
} app_event_type_t;

// Dữ liệu cho sự kiện nhận lệnh OnOff
typedef struct {
    esp_ble_mesh_msg_ctx_t ctx;     // Context của tin nhắn (địa chỉ nguồn, đích, etc.)
    uint8_t onoff_state;            // Trạng thái On/Off được yêu cầu
} event_data_onoff_set_t;
#define MAX_RAW_PAYLOAD_SIZE 64
typedef struct {
    uint16_t address;               /*!< Address of the device that sent the message */
    uint16_t length;                /*!< Length of the received message */
    uint8_t msg[MAX_RAW_PAYLOAD_SIZE]; 
} event_recv_publish_msg_t;

typedef struct {
    app_event_type_t type;
    union {
        event_data_onoff_set_t      onoff_set;
        event_recv_publish_msg_t      publish_status;
    } data;
} app_event_t;


/*
 * =====================================================================================
 * CÁC HÀM API CÔNG KHAI
 * =====================================================================================
 */

/** @brief Khởi tạo hàng đợi sự kiện hệ thống. */
esp_err_t sys_event_queue_init(void);

/** @brief Gửi một sự kiện vào hàng đợi từ một tác vụ (task). */
esp_err_t sys_event_queue_send(const app_event_t *event);

/** @brief Gửi một sự kiện vào hàng đợi từ một ISR. */
esp_err_t sys_event_queue_send_from_isr(const app_event_t *event);

/** @brief Nhận một sự kiện từ hàng đợi (hàm này sẽ block task cho đến khi có sự kiện). */
esp_err_t sys_event_queue_receive(app_event_t *event, TickType_t timeout);

/** @brief Hủy và giải phóng tài nguyên của hàng đợi sự kiện hệ thống. */
void sys_event_queue_deinit(void);

#endif // SYS_EVENT_QUEUE_H