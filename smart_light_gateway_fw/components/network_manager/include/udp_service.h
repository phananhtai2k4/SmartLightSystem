#ifndef UDP_SERVICE_H
#define UDP_SERVICE_H

#include "esp_err.h"
#include <stdint.h>

// UDP service configuration
#define UDP_SERVICE_PORT 8888  // Port mặc định cho UDP service

/**
 * @brief Khởi tạo UDP service
 * 
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t udp_service_init(void);

/**
 * @brief Bắt đầu UDP service task
 * 
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t udp_service_start(void);

/**
 * @brief Dừng UDP service
 * 
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t udp_service_stop(void);

/**
 * @brief Gửi dữ liệu UDP dạng broadcast
 * 
 * @param data Dữ liệu cần gửi
 * @param len Độ dài dữ liệu
 * @param port Port đích (nếu 0 sẽ dùng UDP_SERVICE_PORT)
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t udp_service_send_broadcast1(const char *data, size_t len, uint16_t port);

/**
 * @brief Gửi dữ liệu UDP dạng unicast
 * 
 * @param dest_ip Địa chỉ IP đích (dạng string, ví dụ: "192.168.1.100")
 * @param port Port đích
 * @param data Dữ liệu cần gửi
 * @param len Độ dài dữ liệu
 * @return esp_err_t ESP_OK nếu thành công
 */
esp_err_t udp_service_send_unicast(const char *dest_ip, uint16_t port, const char *data, size_t len);

#endif // UDP_SERVICE_H
