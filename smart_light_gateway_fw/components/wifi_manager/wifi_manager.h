// File: components/wifi_manager/wifi_manager.h

#pragma once

#include "esp_err.h"

/**
 * @brief Khởi tạo trình quản lý Wi-Fi.
 *
 * @return ESP_OK nếu khởi tạo thành công, hoặc mã lỗi khác từ các API bên dưới.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Kết nối đến Wi-Fi với SSID và mật khẩu đã cho.
 *
 * Hàm này sẽ bắt đầu quá trình kết nối đến Wi-Fi.
 * Nếu kết nối thành công, nó sẽ trả về ESP_OK.
 * Nếu không, nó sẽ trả về mã lỗi tương ứng.
 *
 * @param ssid Tên mạng Wi-Fi cần kết nối.
 * @param pass Mật khẩu của mạng Wi-Fi.
 * @return esp_err_t ESP_OK nếu kết nối thành công, hoặc mã lỗi khác nếu thất bại.
 */
esp_err_t wifi_manager_connect_start(const char *ssid, const char *pass);

/**
 * @brief Ngắt kết nối khỏi mạng Wi-Fi hiện tại.
 */
void wifi_manager_disconnect(void);

/**
 * @brief Chờ đợi cho đến khi Wi-Fi có địa chỉ IP.
 *
 * Hàm này sẽ chờ đợi cho đến khi Wi-Fi nhận được địa chỉ IP từ DHCP.
 * Nếu thành công, nó sẽ trả về ESP_OK.
 * Nếu không, nó sẽ trả về mã lỗi tương ứng.
 *
 * @return esp_err_t ESP_OK nếu nhận được địa chỉ IP, hoặc mã lỗi khác nếu thất bại.
 */
esp_err_t wifi_manager_wait_for_ip(void);

/**
 * @brief Chuyển sang chế độ Access Point (AP).
 *
 * Hàm này sẽ cấu hình thiết bị ở chế độ AP với SSID và mật khẩu mặc định từ sdkconfig.
 * @return ESP_OK nếu thành công, hoặc mã lỗi nếu thất bại.
 */
esp_err_t wifi_manager_switch_to_ap(void);

/**
 * @brief Chuyển sang chế độ STA và kết nối với Wi-Fi chỉ định.
 *
 * Hàm này sẽ chuyển sang chế độ STA và kết nối với SSID, mật khẩu cung cấp.
 * @param ssid SSID của Wi-Fi cần kết nối.
 * @param pass Mật khẩu của Wi-Fi cần kết nối.
 * @return ESP_OK nếu thành công, hoặc mã lỗi nếu thất bại.
 */
esp_err_t wifi_manager_switch_to_sta(const char *ssid, const char *pass);
