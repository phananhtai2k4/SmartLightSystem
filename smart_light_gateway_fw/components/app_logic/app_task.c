// components/app_logic/app_task.c

#include "app_event_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_event_queue.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "sys_nvs.h"
#include "task_config.h"
#include "board_led.h"

static const char *TAG = "APP_TASK";

/**
 * @brief Tác vụ chính của ứng dụng, chứa vòng lặp sự kiện.
 */
static void app_main_task(void *arg) {
    ESP_LOGI(TAG, "Tác vụ logic ứng dụng đã bắt đầu.");
    app_event_t startup_event = { .type = EVENT_SYSTEM_STARTUP };
    sys_event_queue_send(&startup_event);

    app_event_t event;
    for (;;) {
        if (sys_event_queue_receive(&event, portMAX_DELAY) == ESP_OK) {
            ESP_LOGI(TAG, "nhận được sự kiện: %d", event.type);
            switch (event.type) {
                case EVENT_SYSTEM_STARTUP:
                    ESP_LOGI(TAG, "Sự kiện: Hệ thống khởi động.");
                    if (boot_flag == BOOT_FLAG_ADDING_GATEWAY_MANUAL) {
                        char ssid[32];
                        char pass[64];
                        esp_err_t nvs_storage_read_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
                        nvs_storage_read_wifi_creds(ssid, sizeof(ssid), pass, sizeof(pass)); // Chỉ để in ra log
                        ESP_LOGI(TAG, "Đang kết nối Wi-Fi với SSID: %s", ssid);
                        wifi_manager_connect_start(ssid, pass);
                    }
                    break;
                
                case EVENT_NETWORK_CONNECTED:
                    handle_network_connected();
                    break;
        
                case EVENT_NETWORK_DISCONNECTED:
                    handle_network_disconnected();
                    break;

                case EVENT_MESH_PROV_COMPLETE:
                    ESP_LOGI(TAG, "Sự kiện: Provisioning hoàn tất.");
                    if (!(boot_flag == BOOT_FLAG_ADDING_GATEWAY_MANUAL)) {
                        // vTaskDelay(pdMS_TO_TICKS(9000));
                        // wifi_manager_connect_start("Cam-hub","0913456789");
                        char ssid[32];
                        char pass[64];
                        esp_err_t nvs_storage_read_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
                        nvs_storage_read_wifi_creds(ssid, sizeof(ssid), pass, sizeof(pass)); // Chỉ để in ra log
                        ESP_LOGI(TAG, "Đang kết nối Wi-Fi với SSID: %s", ssid);
                        wifi_manager_connect_start(ssid, pass);
                    }
                    if (!boot_flags_write(BOOT_FLAG_ADDED_GATEWAY)) {
                        ESP_LOGE(TAG, "FATAL: Could not write boot flag! Aborting reset.");
                    }
                    
                    break;
                    
                case EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED:
                    handle_onoff_set_event(&event.data.onoff_set);
                    break;

                // Button events
                case EVENT_BUTTON_PRESSED:
                    ESP_LOGI(TAG, "Sự kiện: Button được bấm");
                    if (boot_flag == BOOT_FLAG_ADDING_GATEWAY_MANUAL) {
                        boot_flags_write(BOOT_FLAG_NORMAL_START);
                        vTaskDelay(200 / portTICK_PERIOD_MS);
                        esp_restart();
                    }
                    break;

                // case EVENT_BUTTON_RELEASED:
                //     ESP_LOGI(TAG, "Sự kiện: Button được thả");
                //     // Xử lý khi button được thả (nếu cần)
                //     break;

                case EVENT_BUTTON_HOLD_3S:
                    ESP_LOGI(TAG, "Sự kiện: Button giữ 3 giây");
                    wifi_manager_switch_to_ap();
                    break;

                case EVENT_BUTTON_HOLD_5S:
                    ESP_LOGI(TAG, "Sự kiện: Button giữ 5 giây");
                    led_service_set_boot_flag(BOOT_FLAG_NORMAL_START);
                    break;

                default:
                    ESP_LOGW(TAG, "Sự kiện không xác định: %d", event.type);
                    break;
            }
        }
    }
}

esp_err_t app_logic_init(void) {
    if (sys_event_queue_init() != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo hàng đợi sự kiện thất bại. Dừng chương trình.");
        return ESP_FAIL;
    }
    BaseType_t res = xTaskCreatePinnedToCore(app_main_task, app_logic_task_config.name, app_logic_task_config.stack_size, NULL, app_logic_task_config.priority, NULL, app_logic_task_config.core_id);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Tạo tác vụ logic chính thất bại!");
        return ESP_FAIL;
    }

    

    ESP_LOGI(TAG, "Khởi tạo App Logic thành công.");
    return ESP_OK;
}