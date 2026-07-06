/*
 * app_logic.h - Giao diện công khai cho Lớp Logic Ứng dụng
 *
 * File này định nghĩa các hàm để khởi tạo và quản lý "bộ não" của thiết bị.
 */

#ifndef __APP_LOGIC_H__
#define __APP_LOGIC_H__

#include "esp_err.h"

/**
 * @brief Khởi tạo Lớp Logic Ứng dụng.
 *
 * Hàm này sẽ tạo ra tác vụ chính (main task) của ứng dụng, có nhiệm vụ
 * lắng nghe và xử lý các sự kiện từ hàng đợi hệ thống.
 *
 * @return esp_err_t ESP_OK nếu tác vụ được tạo thành công.
 */
esp_err_t app_logic_init(void);

#endif // __APP_LOGIC_H__