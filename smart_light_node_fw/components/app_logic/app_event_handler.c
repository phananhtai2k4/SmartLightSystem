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
    board_led_set_state(STATUS_LED, onoff_data->onoff_state ? LED_STATE_ON : LED_STATE_OFF);
    
    // 3. Lưu trạng thái mới vào NVS để khôi phục sau khi khởi động lại
    relay_state_t state_to_save = { .onoff = onoff_data->onoff_state };
    esp_err_t err = sys_nvs_save_relay_state(&state_to_save);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể lưu trạng thái relay vào NVS!");
    }
}

/**
 * @brief Xử lý một sự kiện nhấn nút.
 */
static void handle_button_press_event(void)
{
    ESP_LOGI(TAG, "Xử lý sự kiện nhấn nút...");

    // Logic: Đảo ngược trạng thái hiện tại của relay
    uint8_t new_state = !onoff_server.state.onoff;

    // 1. Cập nhật trạng thái logic của model
    onoff_server.state.onoff = new_state;

    // 2. Điều khiển phần cứng vật lý
    // board_relay_set_state(new_state);
    board_led_set_state(RELAY_STATE_LED, new_state ? LED_STATE_ON : LED_STATE_OFF);

    // 3. Lưu trạng thái mới vào NVS
    relay_state_t state_to_save = { .onoff = new_state };
    sys_nvs_save_relay_state(&state_to_save);

    // 4. Publish trạng thái mới lên mạng để các thiết bị khác đồng bộ
    ble_mesh_core_publish_onoff_status();
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
        relay_state_t saved_state;
        if (sys_nvs_load_relay_state(&saved_state) == ESP_OK) {
            ESP_LOGI(TAG, "Đã tìm thấy trạng thái đã lưu, khôi phục onoff = %d", saved_state.onoff);
            onoff_server.state.onoff = saved_state.onoff;
            board_led_set_state(RELAY_STATE_LED, saved_state.onoff ? LED_STATE_ON : LED_STATE_OFF);
        } else {
            ESP_LOGI(TAG, "Không có trạng thái nào được lưu, dùng trạng thái mặc định: TẮT");
            onoff_server.state.onoff = 0;
            board_led_set_state(RELAY_STATE_LED, LED_STATE_OFF);
        }
        break;

    case EVENT_MESH_PROV_COMPLETE:
        ESP_LOGI(TAG, "Sự kiện: Provisioning hoàn tất.");
        board_led_set_state(STATUS_LED, LED_STATE_ON); // Bật đèn xanh báo đã kết nối
        break;
        
    case EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED:
        handle_onoff_set_event(&event->data.onoff_set);
        break;
        
    case EVENT_BUTTON_PRESSED:
        handle_button_press_event();
        break;

    // Thêm các case khác ở đây, ví dụ:
    case EVENT_BUTTON_LONG_PRESS:
        ESP_LOGI(TAG, "Sự kiện: Nhấn giữ lâu. Thực hiện reset factory...");
        // sys_nvs_erase_all();
        // esp_restart();
        break;
        
    default:
        ESP_LOGD(TAG, "Sự kiện chưa được xử lý: %d", event->type);
        break;
    }
}