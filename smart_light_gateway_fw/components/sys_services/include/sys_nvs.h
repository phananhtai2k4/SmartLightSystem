// File: components/nvs_storage/nvs_storage.h
//example: storage/nvs_rw_value
#pragma once

#include "esp_err.h"
#include <stddef.h> // Để có kiểu size_t
#include <stdint.h>
#include <stdbool.h>



typedef enum {
    BOOT_FLAG_ADDING_GATEWAY_MANUAL,
    BOOT_FLAG_ADDED_GATEWAY,
    BOOT_FLAG_NORMAL_START,
} boot_flag_t;

extern boot_flag_t boot_flag;

/**
 * @brief Cấu trúc lưu trữ thông tin cấu hình MQTT.
 */
typedef struct {
    char broker_url[128];
    char username[64];
    char password[128];
    char topic_sub[128];
    char topic_pub[128];
    char topic_alert[128];
} mqtt_config_t;

/**
 * @brief Định nghĩa trạng thái của LED.
 */
typedef enum {
    LED_STATE_OFF = 0, // Tắt
    LED_STATE_ON = 1,  // Bật
} led_state_t;

/**
 * @brief Định danh các LED để dễ dàng sử dụng trong các hàm.
 */
typedef enum {
    LED_ID_1,
    LED_ID_2,
    LED_ID_3,
    LED_ID_MAX // Dùng để xác định số lượng LED
} led_id_t;

/**
 * @brief Cấu trúc cho cài đặt của từng công tắc.
 */
typedef struct {
    uint8_t type;               // 1: On/Off, 2: Countdown
    uint32_t countdown_s;       // Thời gian countdown (giây)
    uint8_t control_mode;       // 1: Normal, 2: Disable, 3: Disable Touch, 4: Disable Remote
    uint8_t led_disabled;       // 1: Tắt LED, 0: Hoạt động bình thường (dùng u8 thay vì bool cho chắc chắn về kích thước)
    uint32_t rgb_on;            // Màu LED khi Bật (0-16777215)
    uint32_t rgb_off;           // Màu LED khi Tắt (0-16777215)
    uint8_t lightness;          // Độ sáng (0-100)
} switch_config_t;

/**
 * @brief Cấu trúc chính chứa toàn bộ cấu hình nâng cao của thiết bị.
 */
typedef struct {
    char config_version[32];    // Quan trọng để nâng cấp firmware trong tương lai

    // Cấu hình chung
    uint8_t buzzer_enabled;
    uint8_t led_always_on;      // ledEnb: 1 = luôn sáng, 0 = sáng khi chạm
    uint32_t global_led_rgb_on;
    uint32_t global_led_rgb_off;
    uint8_t global_led_lightness;
    uint8_t save_last_state;    // saveStt

    // Cấu hình Night Mode
    uint8_t night_mode_enabled;
    uint32_t night_begin_ts;
    uint32_t night_end_ts;
    int8_t night_tz;

    // Cấu hình cho 3 công tắc
    switch_config_t switches[3];

} device_config_t;

// Giới hạn số lượng lịch có thể lưu
#define MAX_SCHEDULES 16

// Cấu trúc cho một hành động trong lịch
typedef struct {
    uint8_t switch_id; // 0=switch_1, 1=switch_2, 2=switch_3
    uint8_t value;     // 0=OFF, 1=ON
} schedule_action_t;

// Cấu trúc chính cho một lịch trình
typedef struct {
    uint8_t is_used;          // Lịch này có đang được sử dụng không? (dùng u8)
    uint8_t activate;         // Lịch này có được bật hay không (dùng u8)
    uint8_t loop_days;     // Bitmask cho các ngày lặp lại
    uint8_t num_actions;   // Số lượng hành động
    uint32_t id;           // ID duy nhất của lịch từ app
    uint32_t execution_time_utc; // Timestamp UTC của thời điểm thực thi
    
    schedule_action_t actions[3]; // Tối đa 3 hành động
} schedule_t;

/**
 * @brief Cấu trúc lưu trữ trạng thái của relay vật lý.
 * 
 * Ví dụ đơn giản, có thể mở rộng để lưu nhiều relay.
 */
typedef struct {
    uint8_t onoff; // 0 = OFF, 1 = ON
} relay_state_t;

/**
 * @brief Cấu trúc lưu trữ thông tin mạng BLE Mesh cần thiết.
 * 
 * Các thông tin này giúp node tự động tham gia lại mạng sau khi khởi động.
 */
typedef struct {
    uint16_t net_idx;
    uint16_t app_idx;
    uint16_t primary_addr;
    uint8_t  dev_key[16];
} ble_mesh_info_t;

esp_err_t nvs_storage_init(void);

/**
 * @brief Ghi thông tin đăng nhập Wi-Fi vào NVS.
 *
 * @param[in] ssid SSID của mạng Wi-Fi.
 * @param[in] pass Mật khẩu của mạng Wi-Fi.
 *
 * @return
 *      - ESP_OK nếu ghi thành công
 *      - ESP_FAIL nếu có lỗi
 *      - hoặc các mã lỗi khác từ API NVS
 */
esp_err_t nvs_storage_write_wifi_creds(const char *ssid, const char *pass);

/**
 * @brief Đọc thông tin đăng nhập Wi-Fi từ NVS.
 *
 * @param[out] ssid Buffer để lưu trữ SSID đã đọc.
 * @param[in]  ssid_len Kích thước của buffer SSID.
 * @param[out] pass Buffer để lưu trữ mật khẩu đã đọc.
 * @param[in]  pass_len Kích thước của buffer mật khẩu.
 *
 * @return
 *      - ESP_OK nếu đọc thành công
 *      - ESP_ERR_NVS_NOT_FOUND nếu không tìm thấy thông tin
 *      - hoặc các mã lỗi khác từ API NVS
 */
esp_err_t nvs_storage_read_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len);

/**
 * @brief Ghi cấu hình MQTT vào NVS.
 *
 * @param[in] config Cấu hình MQTT cần ghi.
 * @return
 *      - ESP_OK nếu ghi thành công
 *      - ESP_FAIL nếu có lỗi
 *      - hoặc các mã lỗi khác từ API NVS
 */
esp_err_t nvs_storage_write_mqtt_config(const mqtt_config_t *config);

/**
 * @brief Đọc cấu hình MQTT từ NVS.
 *
 * @param[out] config Con trỏ đến cấu trúc mqtt_config_t để lưu trữ cấu hình đọc được.
 * @return
 *      - ESP_OK nếu đọc thành công
 *      - ESP_ERR_NVS_NOT_FOUND nếu không tìm thấy cấu hình
 *      - hoặc các mã lỗi khác từ API NVS
 */
esp_err_t nvs_storage_read_mqtt_config(mqtt_config_t *config);

/**
 * @brief Xóa cấu hình MQTT khỏi NVS.
 *
 * @return
 *      - ESP_OK nếu xóa thành công
 *      - ESP_FAIL nếu có lỗi
 *      - hoặc các mã lỗi khác từ API NVS
 */
esp_err_t nvs_storage_clear_mqtt_config(void);

/**
 * @brief Xóa một khóa (key) cụ thể khỏi NVS.
 * 
 * Hữu ích khi cần reset một phần cấu hình.
 * 
 * @param key Chuỗi ký tự tên của khóa cần xóa.
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t sys_nvs_erase_key(const char *key);

/**
 * @brief Xóa toàn bộ namespace của ứng dụng.
 * 
 * Thường dùng cho chức năng factory reset.
 * 
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t sys_nvs_erase_all(void);

/**
 * @brief Ghi một cờ vào phân vùng 'flags'.
 * 
 * @param flag Cờ cần ghi.
 * @return true nếu ghi thành công.
 * @return false nếu thất bại.
 */
bool boot_flags_write(boot_flag_t flag);

/**
 * @brief Đọc cờ từ phân vùng 'flags'.
 * 
 * @return boot_flag_t Cờ đã được đọc. Sẽ trả về BOOT_FLAG_NORMAL_START nếu đọc thất bại hoặc
 *                     phân vùng còn trống (toàn byte 0xFF).
 */
boot_flag_t boot_flags_read(void);