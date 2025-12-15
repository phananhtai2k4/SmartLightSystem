// File: main/main.c

#include "esp_log.h"
#include "esp_event.h"

// Include các component cần khởi tạo nền tảng
#include "sys_nvs.h"
#include "wifi_manager.h"
#include "esp_idf_version.h" 
#include "esp_system.h"
#include "sys_event_queue.h"
#include "app_event_handler.h"

#include "ble_mesh_core.h"
#include "bluetooth_init.h"
#include "cmd_test.h"

static const char *TAG = "MAIN";

void app_main(void)
{ 
    ESP_LOGI(TAG, "[APP] Application starting...");
    ESP_ERROR_CHECK(nvs_storage_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    cmd_test_start();

    esp_err_t app_init_result = app_logic_init();

    if (app_init_result != ESP_OK) {
        ESP_LOGE(TAG, "KHỞI TẠO LOGIC ỨNG DỤNG THẤT BẠI! Mã lỗi: 0x%x", app_init_result);
        return;
    }

    esp_err_t err;

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

    ESP_LOGI(TAG, "Application main task finished.");
}