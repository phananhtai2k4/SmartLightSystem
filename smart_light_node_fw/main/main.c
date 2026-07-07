/*
 * main.c - Điểm khởi đầu của ứng dụng Smart Relay Node
 *
 * Nhiệm vụ duy nhất của hàm app_main là khởi tạo các component
 * hệ thống theo đúng thứ tự.
 */

#include "esp_log.h"

// Nhúng tất cả các file header của các component cần khởi tạo
#include "sys_nvs.h"
#include "sys_event_queue.h"
#include "board_hal.h"
#include "app_logic.h"
#include "ble_mesh_core.h"
#include "bluetooth_init.h"
#include "cmd_test.h"

#define TAG "MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "Bắt đầu khởi tạo Smart Relay Node...");

    esp_err_t err;

    err = sys_nvs_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo NVS thất bại!");
        return; 
    }

    err = sys_event_queue_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo hàng đợi sự kiện thất bại!");
        return;
    }
    cmd_test_start();
    
    err = board_hal_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo Board HAL thất bại!");
        return;
    }

    err = app_logic_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo App Logic thất bại!");
        return;
    }

    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }
    ESP_LOGI(TAG, "Bluetooth đã được khởi tạo.");

    err = ble_mesh_core_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo BLE Mesh Core thất bại!");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    board_adc_rms_init();
    ESP_LOGI(TAG, "Khởi tạo hoàn tất. Thiết bị đang hoạt động.");
}