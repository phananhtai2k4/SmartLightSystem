// File: components/time_manager/time_manager.h
#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include <time.h>

/**
 * @brief Khởi tạo và bắt đầu đồng bộ thời gian qua SNTP.
 *
 * Hàm này không blocking. Nó sẽ cấu hình SNTP và để nó chạy nền.
 *
 * @return esp_err_t 
 */
esp_err_t time_manager_init(void);

/**
 * @brief Kiểm tra xem thời gian đã được đồng bộ ít nhất một lần hay chưa.
 *
 * @return true nếu thời gian đã được đồng bộ, false nếu chưa.
 */
bool time_manager_is_synced(void);

/**
 * @brief Lấy timestamp UNIX hiện tại (số giây từ 1970-01-01 00:00:00 UTC).
 *
 * @return time_t Timestamp hiện tại. Trả về 0 nếu thời gian chưa được đồng bộ.
 */
time_t time_manager_get_timestamp(void);

#endif