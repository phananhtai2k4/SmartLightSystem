/*
 * app_task.c - Tác vụ chính và vòng lặp sự kiện của ứng dụng
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_logic.h"
#include "sys_event_queue.h"

#define TAG "APP_LOGIC_TASK"
#define APP_TASK_STACK_SIZE 4096

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

    app_event_t startup_event = { .type = EVENT_SYSTEM_STARTUP };
    sys_event_queue_send(&startup_event);
    
    app_event_t event;
    for (;;) {
        if (sys_event_queue_receive(&event, portMAX_DELAY)) {
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
    BaseType_t res = xTaskCreate(
        app_main_task,
        "app_task",
        APP_TASK_STACK_SIZE,
        NULL,
        5,
        NULL
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Không thể tạo tác vụ logic ứng dụng!");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}