#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>
#include "sys_nvs.h"

/**
 * @brief Khởi tạo module điều khiển thiết bị.
 * 
 * Module này sẽ đăng ký vào topic command để nhận lệnh từ server.
 *
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t device_control_init(void);

#endif