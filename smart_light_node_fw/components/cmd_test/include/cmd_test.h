// components/cmd_test/cmd_test.h
#ifndef CMD_TEST_H
#define CMD_TEST_H

#include "esp_err.h"

/**
 * @brief Khởi động và chạy giao diện dòng lệnh (CLI) để test.
 * 
 * Hàm này sẽ khởi tạo esp_console, đăng ký các lệnh test,
 * và bắt đầu vòng lặp REPL. Hàm này sẽ không quay trở lại.
 *
 * @return 
 *  - ESP_OK: nếu khởi tạo và bắt đầu thành công (về mặt lý thuyết, vì nó lặp vô hạn).
 *  - Các mã lỗi khác: nếu có lỗi xảy ra trong quá trình thiết lập.
 */
esp_err_t cmd_test_start(void);

#endif // CMD_TEST_H