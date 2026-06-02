/*
 * app_event_handler.c - Bộ xử lý sự kiện trung tâm của ứng dụng
 *
 * File này chứa logic nghiệp vụ để phản ứng lại các sự kiện từ
 * phần cứng, mạng BLE Mesh, và các nguồn khác.
 */

#include "esp_log.h"

#include "sys_event_queue.h"    // Để biết các loại sự kiện
#include "board_hal.h"          // Để điều khiển LED
#include "ble_mesh_core.h"      // Để gửi tin nhắn BLE Mesh
#include "sys_nvs.h"            // Để lưu/đọc trạng thái

#define TAG "APP_LOGIC_HANDLER"

// Khai báo biến extern để truy cập trực tiếp vào trạng thái của OnOff Server model
// Điều này cần thiết để cập nhật trạng thái logic của model.
extern esp_ble_mesh_gen_onoff_srv_t onoff_server;
/**
 * @brief Xử lý một sự kiện OnOff Set nhận được từ mạng BLE Mesh.
 */
static void handle_onoff_set_event(const event_data_onoff_set_t *onoff_data)
{
    ESP_LOGI(TAG, "Xử lý sự kiện OnOff Set: state=%d từ src=0x%04x", 
             onoff_data->onoff_state, onoff_data->ctx.addr);

    // 1. Cập nhật trạng thái logic của model
    // onoff_server.state.onoff = onoff_data->onoff_state;
    
    // 2. Điều khiển phần cứng vật lý (relay và đèn LED trạng thái)
    // Giả sử có hàm board_relay_set_state() trong board_hal
    // board_relay_set_state(onoff_data->onoff_state); 
    board_led_set_state(RELAY_STATE_LED, onoff_data->onoff_state ? LED_STATE_ON : LED_STATE_OFF);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ble_mesh_core_publish_onoff_status_by_app();
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/**
 * @brief Xử lý một sự kiện nhấn nút.
 */
static void handle_button_press_event(void)
{
    ESP_LOGI(TAG, "Xử lý sự kiện nhấn nút...");

    // Logic: Đảo ngược trạng thái hiện tại của relay
    uint8_t new_state = board_led_get_relay_state();

    // 1. Cập nhật trạng thái logic của model
    onoff_server.state.onoff = new_state;

    // 4. Publish trạng thái mới lên mạng để các thiết bị khác đồng bộ
    vTaskDelay(50 / portTICK_PERIOD_MS);
    ble_mesh_core_publish_onoff_status_by_button();
    vTaskDelay(50 / portTICK_PERIOD_MS);
}


/**
 * @brief Hàm điều phối chính, xử lý tất cả các sự kiện của ứng dụng.
 * 
 * @param event Con trỏ tới sự kiện cần xử lý.
 */
void app_event_handler(const app_event_t *event)
{
    switch (event->type) {
    case EVENT_SYSTEM_STARTUP:
        ESP_LOGI(TAG, "Sự kiện: Hệ thống khởi động.");
        // Khi khởi động, đọc trạng thái đã lưu từ NVS
        if (sys_nvs_load_device_status(&saved_status) == ESP_OK) {
            ESP_LOGI(TAG, "Đã tìm thấy trạng thái đã lưu, khôi phục onoff = %d, lux=%d, current=%dmA", saved_status.onoff, saved_status.light_lux, saved_status.current_ma);
            onoff_server.state.onoff = saved_status.onoff;
            board_led_set_state(RELAY_STATE_LED, saved_status.onoff ? LED_STATE_ON : LED_STATE_OFF);
        } else {
            ESP_LOGI(TAG, "Không có trạng thái nào được lưu, dùng trạng thái mặc định: TẮT");
            onoff_server.state.onoff = 0;
            board_led_set_state(RELAY_STATE_LED, LED_STATE_OFF);
        }
        break;

    case EVENT_MESH_PROV_COMPLETE:
        ESP_LOGI(TAG, "Sự kiện: Provisioning hoàn tất.");
        boot_flags_write(BOOT_FLAG_ADDED_DEVICE);
        ble_mesh_composition_setup_defaults();
        break;
        
    case EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED:
        handle_onoff_set_event(&event->data.onoff_set);
        break;
        
    case EVENT_BUTTON_PRESSED:
        if (boot_flag == BOOT_FLAG_ADDING_DEVICE_MANUAL) {
                boot_flags_write(BOOT_FLAG_NORMAL_START);
                vTaskDelay(200 / portTICK_PERIOD_MS);
                esp_restart();
        }
        handle_button_press_event();
        break;

    // Thêm các case khác ở đây, ví dụ:
    case EVENT_BUTTON_HOLD_3S:
        ESP_LOGI(TAG, "Sự kiện: Nhấn giữ nút 3 giây");
        boot_flags_write(BOOT_FLAG_ADDING_DEVICE_MANUAL);
        ble_mesh_core_node_local_reset();
        ble_mesh_core_node_prov_enable();
        break;
        
    case EVENT_BUTTON_HOLD_5S:
        ESP_LOGI(TAG, "Sự kiện: Nhấn giữ nút 5 giây");
        // Thực hiện chức năng khác nếu cần
        break;
        
    default:
        ESP_LOGW(TAG, "Sự kiện chưa được xử lý: %d", event->type);
        break;
    }
}