/*
 * board_hal.h - Lớp trừu tượng phần cứng (HAL) cho Smart Relay Node
 *
 * File header này định nghĩa giao diện công khai để tương tác với các
 * phần cứng đặc thù của bo mạch, ví dụ như LED và nút bấm.
 */

#ifndef __BOARD_HAL_H__
#define __BOARD_HAL_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"

#define BUTTON_GPIO     22

// Định nghĩa các chân GPIO cho mỗi đèn LED chức năng
#define STATUS_LED_GPIO         GPIO_NUM_2
#define RELAY_STATE_LED_GPIO    GPIO_NUM_4

/*
 * @brief Định danh cho các đèn LED cụ thể trên bo mạch.
 *
 * Điều này cho phép logic ứng dụng điều khiển LED dựa trên chức năng của chúng
 * thay vì màu sắc vật lý hay chân GPIO.
 */
typedef enum {
    STATUS_LED,         // LED chỉ thị trạng thái mạng (ví dụ: đang provision, đã kết nối)
    RELAY_STATE_LED,    // LED chỉ thị trạng thái vật lý của relay (Bật/Tắt)
    BOARD_LED_ID_MAX    // Dùng để xác định số lượng LED, không phải là một ID thực
} board_led_id_t;

/*
 * @brief Các trạng thái mong muốn của đèn LED.
 */
typedef enum {
    LED_STATE_OFF,          // Trạng thái Tắt
    LED_STATE_ON,           // Trạng thái Bật
    LED_STATE_BLINK_FAST,   // Trạng thái Nhấp nháy nhanh
    LED_STATE_BLINK_SLOW,   // Trạng thái Nhấp nháy chậm
} board_led_state_t;


/**
 * @brief Khởi tạo tất cả các phần cứng đặc thù của bo mạch.
 *
 * Hàm này nên được gọi một lần duy nhất khi ứng dụng khởi động. Nó sẽ
 * khởi tạo GPIO cho LED, nút bấm và bất kỳ ngoại vi nào khác.
 *
 * @return esp_err_t ESP_OK nếu thành công, hoặc mã lỗi nếu thất bại.
 */
esp_err_t board_hal_init(void);
esp_err_t board_adc_init(void);

/**
 * @brief Thiết lập trạng thái của một đèn LED chức năng cụ thể.
 *
 * Logic ứng dụng sẽ sử dụng hàm này để điều khiển LED mà không cần phải
 * biết về GPIO hay timer bên dưới.
 *
 * @param led_id Định danh chức năng của đèn LED cần điều khiển.
 * @param state Trạng thái mong muốn cho đèn LED (Bật, Tắt, Nhấp nháy).
 *
 * @return esp_err_t ESP_OK nếu thành công, hoặc mã lỗi nếu led_id không hợp lệ.
 */
esp_err_t board_led_set_state(board_led_id_t led_id, board_led_state_t state);

void boot_flag_change_process();
// Chú ý: Các khai báo hàm liên quan đến nút bấm cũng sẽ được đặt ở đây.
// Ví dụ:
// typedef void (*board_button_callback_t)(uint8_t button_id, board_button_event_t event);
// esp_err_t board_button_register_callback(board_button_callback_t cb);
esp_err_t board_led_change_relay_state(void);

void board_adc_rms_init(void);
#endif // __BOARD_HAL_H__