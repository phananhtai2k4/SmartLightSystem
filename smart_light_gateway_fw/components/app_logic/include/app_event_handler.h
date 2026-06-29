// components/app_logic/include/app_logic.h

#ifndef APP_EVENT_HANDLER_H
#define APP_EVENT_HANDLER_H

#include "esp_err.h"
#include "sys_event_queue.h"

#define IS_COMPLEX_NODE 1

/*
 * @brief Các trạng thái nhận được của đèn LED.
 */
typedef enum {
    OFF_LED_BY_BUTTON = 0,      // tắt từ nút cứng
    ON_LED_BY_BUTTON = 1,       // bật từ nút cứng
    OFF_LED_BY_APP = 2,         // tắt từ ứng dụng
    ON_LED_BY_APP = 3,          // bật từ ứng dụng
    OFF_LED_BY_LEAKAGE = 4,     // tắt từ dòng rò
    ON_LED_BY_LEAKAGE = 5,      // bật từ dòng rò
} board_led_state_get_t;


/**
 * @brief Khởi tạo và bắt đầu tác vụ logic chính của ứng dụng.
 *
 * Hàm này sẽ khởi tạo các module phụ thuộc (như event queue, button HAL)
 * và tạo ra task chính để xử lý vòng lặp sự kiện.
 */
esp_err_t app_logic_init(void);

void handle_network_connected(void);

void handle_network_disconnected(void);

void handle_onoff_set_event(const event_data_onoff_set_t *onoff_data);
void handle_recv_publish_status(uint8_t *data, uint16_t len, uint16_t address);
#endif // APP_EVENT_HANDLER_H