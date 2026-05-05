/*
 * app_task.c - Tác vụ chính và vòng lặp sự kiện của ứng dụng
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_logic.h"
#include "sys_event_queue.h"

#define TAG "APP_LOGIC_TASK"
#define APP_TASK_STACK_SIZE 4096 // Kích thước stack cho tác vụ

// Khai báo hàm xử lý sự kiện từ file khác
extern void app_event_handler(const app_event_t *event);

/**
 * @brief Tác vụ chính của ứng dụng.
 *
 * Tác vụ này chạy một vòng lặp vô tận, chờ đợi sự kiện từ hàng đợi chung
 * và chuyển chúng đến bộ xử lý sự kiện.
 *
 * @param arg Tham số truyền vào khi tạo tác vụ (không sử dụng).
 */
static void app_main_task(void *arg)
{
    ESP_LOGI(TAG, "Tác vụ logic ứng dụng đã bắt đầu.");

    // Gửi sự kiện khởi động đầu tiên để thực hiện các hành động ban đầu
    app_event_t startup_event = { .type = EVENT_SYSTEM_STARTUP };
    sys_event_queue_send(&startup_event);
    
    app_event_t event;
    for (;;) {
        // Chờ nhận sự kiện từ hàng đợi.
        // Tác vụ sẽ bị block (ngủ) ở đây cho đến khi có sự kiện mới,
        // không tốn CPU.
        if (sys_event_queue_receive(&event, portMAX_DELAY)) {
            // ESP_LOGI(TAG, "Đã nhận được sự kiện, type = %d. Bắt đầu xử lý...", event.type);
            // Chuyển sự kiện đến bộ xử lý
            app_event_handler(&event);
            // ESP_LOGI(TAG, "Xử lý sự kiện hoàn tất.");
        }
    }
}

/**
 * @brief Khởi tạo Lớp Logic Ứng dụng.
 */
esp_err_t app_logic_init(void)
{
    // Tạo tác vụ chính
    BaseType_t res = xTaskCreate(
        app_main_task,          // Hàm của tác vụ
        "app_task",             // Tên tác vụ (dùng để debug)
        APP_TASK_STACK_SIZE,    // Kích thước stack
        NULL,                   // Tham số truyền vào
        5,                      // Độ ưu tiên
        NULL                    // Handle của tác vụ (không cần)
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Không thể tạo tác vụ logic ứng dụng!");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}