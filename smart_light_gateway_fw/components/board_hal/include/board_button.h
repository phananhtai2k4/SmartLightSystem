#ifndef BOARD_BUTTON_H
#define BOARD_BUTTON_H

#include "esp_err.h"

// Button GPIO Pin
#define BUTTON_GPIO     0

/**
 * @brief Khởi tạo button service và tạo task scan
 * @return ESP_OK on success
 */
esp_err_t button_service_init(void);

#endif // BOARD_BUTTON_H
