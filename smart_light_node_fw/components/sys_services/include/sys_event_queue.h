/*
 * sys_event_queue.h - Giao diện công khai cho Hàng đợi sự kiện hệ thống
 *
 * File này định nghĩa cấu trúc sự kiện chung và các hàm để gửi/nhận
 * sự kiện giữa các component khác nhau trong ứng dụng một cách phi đồng bộ.
 */

#ifndef __SYS_EVENT_QUEUE_H__
#define __SYS_EVENT_QUEUE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>
#include "esp_ble_mesh_defs.h"

// Định nghĩa tất cả các loại sự kiện có thể xảy ra trong hệ thống.
// Việc định nghĩa tập trung giúp dễ dàng quản lý và mở rộng.
typedef enum {
    // --- Sự kiện từ nguồn Phần cứng (Board HAL) ---
    EVENT_BUTTON_PRESSED,
    EVENT_BUTTON_RELEASED,
    EVENT_BUTTON_HOLD_3S,
    EVENT_BUTTON_HOLD_5S,

    // --- Sự kiện từ nguồn BLE Mesh Core ---
    EVENT_MESH_PROV_COMPLETE,           // Provisioning hoàn tất
    EVENT_MESH_CONFIG_RECEIVED,         // Nhận được thông tin cấu hình (ví dụ: AppKey bind)
    EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED, // Nhận được lệnh OnOff Set/SetUnack

    // --- Sự kiện từ nguồn Networking (cho Gateway) ---
    // EVENT_WIFI_CONNECTED,
    // EVENT_WIFI_DISCONNECTED,
    // EVENT_MQTT_CMD_RECEIVED,

    // --- Các sự kiện hệ thống khác ---
    EVENT_SYSTEM_STARTUP,
    //phát hiện dòng rò
    EVENT_LEAKAGE_CURRENT_DETECTED,
} app_event_type_t;

// --- Định nghĩa cấu trúc dữ liệu đi kèm cho từng loại sự kiện ---

// Dữ liệu cho sự kiện nút bấm
typedef struct {
    uint8_t button_id; // ID của nút bấm (nếu có nhiều nút)
} event_data_button_t;

// Dữ liệu cho sự kiện nhận lệnh OnOff
typedef struct {
    esp_ble_mesh_msg_ctx_t ctx;     // Context của tin nhắn (địa chỉ nguồn, đích, etc.)
    uint8_t onoff_state;            // Trạng thái On/Off được yêu cầu
} event_data_onoff_set_t;

// Cấu trúc sự kiện chính, sử dụng union để tiết kiệm bộ nhớ.
typedef struct {
    app_event_type_t type;  // Loại sự kiện
    union {
        // Mỗi loại sự kiện có thể có dữ liệu đi kèm riêng
        event_data_button_t         button;
        event_data_onoff_set_t      onoff_set;
        // Thêm các loại dữ liệu khác ở đây
        // event_data_mqtt_cmd_t    mqtt_cmd;
    } data;
} app_event_t;


/**
 * @brief Khởi tạo hàng đợi sự kiện hệ thống.
 *
 * Phải được gọi một lần trước khi bất kỳ component nào cố gắng gửi sự kiện.
 *
 * @return esp_err_t ESP_OK nếu thành công, hoặc mã lỗi nếu thất bại.
 */
esp_err_t sys_event_queue_init(void);

/**
 * @brief Gửi một sự kiện vào hàng đợi.
 *
 * Hàm này an toàn để gọi từ bất kỳ tác vụ (task) nào hoặc từ Trình phục vụ ngắt (ISR).
 *
 * @param event Con trỏ tới sự kiện cần gửi. Dữ liệu sẽ được sao chép vào hàng đợi.
 *
 * @return bool true nếu gửi thành công, false nếu hàng đợi đầy.
 */
bool sys_event_queue_send(const app_event_t *event);

/**
 * @brief Gửi một sự kiện vào hàng đợi từ một ISR.
 *
 * Phiên bản đặc biệt của hàm send để sử dụng bên trong các ISR.
 *
 * @param event Con trỏ tới sự kiện cần gửi.
 *
 * @return bool true nếu gửi thành công, false nếu hàng đợi đầy.
 */
bool sys_event_queue_send_from_isr(const app_event_t *event);

/**
 * @brief Nhận một sự kiện từ hàng đợi.
 *
 * Hàm này sẽ block (chặn) tác vụ đang chạy cho đến khi có sự kiện mới
 * hoặc hết thời gian chờ.
 *
 * @param event Con trỏ tới một cấu trúc để lưu trữ sự kiện nhận được.
 * @param timeout Thời gian chờ tối đa (tính bằng tick). Sử dụng portMAX_DELAY để chờ vô hạn.
 *
 * @return bool true nếu nhận được sự kiện, false nếu hết thời gian chờ.
 */
bool sys_event_queue_receive(app_event_t *event, TickType_t timeout);

#endif // __SYS_EVENT_QUEUE_H__