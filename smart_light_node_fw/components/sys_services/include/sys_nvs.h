/*
 * sys_nvs.h - Giao diện công khai cho dịch vụ lưu trữ NVS
 *
 * File này cung cấp các hàm và cấu trúc dữ liệu để lưu và khôi phục
 * thông tin cấu hình và trạng thái của ứng dụng một cách bền bỉ
 * qua các lần khởi động lại.
 */

#ifndef __SYS_NVS_H__
#define __SYS_NVS_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/*
 * =====================================================================================
 * CÁC CẤU TRÚC DỮ LIỆU CẦN LƯU TRỮ
 * =====================================================================================
 */

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
    uint16_t primary_addr; // Địa chỉ Unicast của node
    uint8_t  dev_key[16];  // DevKey rất quan trọng để cấu hình lại nếu cần
} ble_mesh_info_t;


/*
 * =====================================================================================
 * CÁC HÀM API CÔNG KHAI
 * =====================================================================================
 */

/**
 * @brief Khởi tạo dịch vụ NVS.
 *
 * Bao gồm việc khởi tạo NVS flash và mở namespace của ứng dụng.
 * Phải được gọi một lần khi khởi động.
 *
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t sys_nvs_init(void);

/**
 * @brief Lưu trạng thái hiện tại của relay vào NVS.
 *
 * @param state Con trỏ tới cấu trúc chứa trạng thái cần lưu.
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t sys_nvs_save_relay_state(const relay_state_t *state);

/**
 * @brief Đọc trạng thái relay đã lưu từ NVS.
 *
 * @param state Con trỏ tới một cấu trúc để điền dữ liệu đọc được.
 * @return esp_err_t ESP_OK nếu tìm thấy và đọc thành công,
 *                   ESP_ERR_NVS_NOT_FOUND nếu chưa có dữ liệu,
 *                   hoặc mã lỗi khác nếu thất bại.
 */
esp_err_t sys_nvs_load_relay_state(relay_state_t *state);

/**
 * @brief Lưu thông tin mạng BLE Mesh vào NVS.
 *
 * @param info Con trỏ tới cấu trúc chứa thông tin BLE Mesh cần lưu.
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t sys_nvs_save_ble_mesh_info(const ble_mesh_info_t *info);

/**
 * @brief Đọc thông tin mạng BLE Mesh đã lưu từ NVS.
 *
 * @param info Con trỏ tới một cấu trúc để điền dữ liệu đọc được.
 * @return esp_err_t ESP_OK nếu tìm thấy và đọc thành công,
 *                   ESP_ERR_NVS_NOT_FOUND nếu chưa có dữ liệu.
 */
esp_err_t sys_nvs_load_ble_mesh_info(ble_mesh_info_t *info);

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

#endif // __SYS_NVS_H__