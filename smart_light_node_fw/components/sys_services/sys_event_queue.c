/*
 * sys_event_queue.c - Triển khai Hàng đợi sự kiện hệ thống
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sys_event_queue.h" // Nhúng file header công khai

#define TAG "SYS_EVENT_QUEUE"

// --- Các định nghĩa cấu hình cho hàng đợi ---
#define EVENT_QUEUE_LENGTH    10 // Số lượng sự kiện tối đa có thể chứa trong hàng đợi
#define EVENT_QUEUE_ITEM_SIZE sizeof(app_event_t)

static QueueHandle_t app_event_queue = NULL;

esp_err_t sys_event_queue_init(void)
{
    if (app_event_queue) {
        // Hàng đợi đã được khởi tạo rồi
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Đang khởi tạo hàng đợi sự kiện hệ thống (kích thước: %d)", EVENT_QUEUE_LENGTH);
    app_event_queue = xQueueCreate(EVENT_QUEUE_LENGTH, EVENT_QUEUE_ITEM_SIZE);

    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Không thể tạo hàng đợi sự kiện!");
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool sys_event_queue_send(const app_event_t *event)
{
    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Hàng đợi chưa được khởi tạo!");
        return false;
    }
    
    // Gửi một bản sao của sự kiện vào hàng đợi.
    // Tham số cuối cùng (0) có nghĩa là không chờ nếu hàng đợi đầy, trả về lỗi ngay.
    if (xQueueSend(app_event_queue, event, 0) != pdPASS) {
        ESP_LOGW(TAG, "Không thể gửi sự kiện vào hàng đợi. Hàng đợi có thể đã đầy.");
        return false;
    }

    return true;
}

bool sys_event_queue_send_from_isr(const app_event_t *event)
{
    if (app_event_queue == NULL) {
        return false;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(app_event_queue, event, &xHigherPriorityTaskWoken) != pdPASS) {
        // Hàng đợi đầy
        return false;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
    
    return true;
}

bool sys_event_queue_receive(app_event_t *event, TickType_t timeout)
{
    if (app_event_queue == NULL) {
        ESP_LOGE(TAG, "Hàng đợi chưa được khởi tạo!");
        return false;
    }

    if (xQueueReceive(app_event_queue, event, timeout) != pdPASS) {
        // Hết thời gian chờ, không có sự kiện nào
        return false;
    }

    return true;
}