// components/sys_services/sys_event_queue.c

#include "sys_event_queue.h"
#include "esp_log.h"

static const char *TAG = "SYS_EVENT_QUEUE";

// Hàng đợi sự kiện trung tâm của toàn bộ ứng dụng.
static QueueHandle_t g_sys_event_queue = NULL;

#define SYS_EVENT_QUEUE_LENGTH  10

/*
 * =====================================================================================
 * CÁC HÀM TIỆN ÍCH NỘI BỘ (Generic Queue Helpers)
 * =====================================================================================
 */

/** @brief Tạo một hàng đợi mới. */
static QueueHandle_t _queue_create(const UBaseType_t queue_length, const UBaseType_t item_size) {
    QueueHandle_t queue = xQueueCreate(queue_length, item_size);
    if (queue == NULL) {
        ESP_LOGE(TAG, "Không thể tạo hàng đợi! Không đủ bộ nhớ.");
    } else {
        ESP_LOGI(TAG, "Một hàng đợi đã được tạo thành công.");
    }
    return queue;
}

/** @brief Gửi một item vào hàng đợi từ một Task. */
static esp_err_t _queue_send(QueueHandle_t queue, const void *item, TickType_t timeout) {
    if (queue == NULL || item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueSend(queue, item, timeout) != pdPASS) {
        //hàng đợi đầy và timeout = 0
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/** @brief Gửi một item vào hàng đợi từ một ISR. */
static esp_err_t _queue_send_from_isr(QueueHandle_t queue, const void *item) {
    if (queue == NULL || item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(queue, item, &xHigherPriorityTaskWoken) != pdPASS) {
        // hàng đợi đầy
        return ESP_FAIL;
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
    return ESP_OK;
}

/** @brief Nhận một item từ hàng đợi. */
static esp_err_t _queue_receive(QueueHandle_t queue, void *item, TickType_t timeout) {
    if (queue == NULL || item == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueReceive(queue, item, timeout) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/** @brief Xóa một hàng đợi. */
static void _queue_delete(QueueHandle_t queue) {
    if (queue != NULL) {
        vQueueDelete(queue);
        ESP_LOGI(TAG, "Một hàng đợi đã được xóa.");
    }
}


/*
 * =====================================================================================
 * GIAO DIỆN CÔNG KHAI (Public API for the System Event Queue)
 * =====================================================================================
 */

esp_err_t sys_event_queue_init(void) {
    if (g_sys_event_queue != NULL) {
        ESP_LOGW(TAG, "Hàng đợi sự kiện hệ thống đã được khởi tạo rồi.");
        return ESP_OK;
    }

    g_sys_event_queue = _queue_create(SYS_EVENT_QUEUE_LENGTH, sizeof(app_event_t));
    if (g_sys_event_queue == NULL) {
        return ESP_FAIL; // _queue_create đã log lỗi
    }
    
    ESP_LOGI(TAG, "Hàng đợi sự kiện hệ thống khởi tạo thành công.");
    return ESP_OK;
}

esp_err_t sys_event_queue_send(const app_event_t *event) {
    esp_err_t err = _queue_send(g_sys_event_queue, event, (TickType_t)0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Gửi sự kiện thất bại, mã lỗi: 0x%x", err);
    }
    return err;
}

esp_err_t sys_event_queue_send_from_isr(const app_event_t *event) {
    return _queue_send_from_isr(g_sys_event_queue, event);
}

esp_err_t sys_event_queue_receive(app_event_t *event, TickType_t timeout) {
    return _queue_receive(g_sys_event_queue, event, timeout);
}

void sys_event_queue_deinit(void) {
    _queue_delete(g_sys_event_queue);
    g_sys_event_queue = NULL;
}