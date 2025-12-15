/*
 * board_button.c - Triển khai logic xử lý nút bấm
 *
 * File này quản lý việc phát hiện các hành động nhấn nút, xử lý chống dội
 * và gửi các sự kiện tương ứng đến hàng đợi hệ thống.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "board_hal.h"          // Header công khai của HAL
#include "sys_event_queue.h"    // Header của hàng đợi sự kiện

#define TAG "BOARD_HAL_BUTTON"

/*
 * =====================================================================================
 * PHẦN 1: CÁC ĐỊNH NGHĨA PHỤ THUỘC PHẦN CỨNG VÀ CẤU HÌNH
 * =====================================================================================
 */

// Định nghĩa chân GPIO cho nút bấm chính
#define MAIN_BUTTON_GPIO            GPIO_NUM_0

// Cấu hình thời gian
#define DEBOUNCE_PERIOD_MS          50      // Thời gian chống dội: 50ms
#define LONG_PRESS_PERIOD_MS        2000    // Thời gian để coi là một lần nhấn giữ lâu: 2 giây

/*
 * =====================================================================================
 * PHẦN 2: CẤU TRÚC DỮ LIỆU NỘI BỘ VÀ BIẾN STATIC
 * =====================================================================================
 */

// Cấu trúc nội bộ để quản lý trạng thái của một nút bấm
typedef struct {
    gpio_num_t          gpio_num;           // Chân GPIO của nút
    TimerHandle_t       debounce_timer;     // Timer cho việc chống dội
    TimerHandle_t       long_press_timer;   // Timer cho việc phát hiện nhấn giữ lâu
    bool                is_pressed;         // Cờ để theo dõi trạng thái đang được nhấn
} button_control_t;

// Mảng quản lý tất cả các nút bấm. Hiện tại chỉ có 1.
// Có thể dễ dàng mở rộng cho nhiều nút.
static button_control_t button_config[] = {
    [0] = { .gpio_num = MAIN_BUTTON_GPIO },
};

#define NUM_BUTTONS (sizeof(button_config) / sizeof(button_control_t))


/*
 * =====================================================================================
 * PHẦN 3: CÁC HÀM RIÊNG TƯ (STATIC) - ISR VÀ CALLBACK CỦA TIMER
 * =====================================================================================
 */

/**
 * @brief Hàm callback của timer, được gọi sau khi hết thời gian chống dội.
 *
 * Hàm này sẽ đọc trạng thái ổn định của nút bấm và gửi sự kiện tương ứng.
 */
static void debounce_timer_callback(TimerHandle_t xTimer)
{
    // Tìm xem timer này thuộc về nút bấm nào (dùng ID được lưu trong timer)
    uint32_t button_idx = (uint32_t)pvTimerGetTimerID(xTimer);
    button_control_t *button = &button_config[button_idx];

    // Đọc trạng thái hiện tại của chân GPIO
    int current_level = gpio_get_level(button->gpio_num);

    if (current_level == 0) { // Nút đang được nhấn (giả sử active-low)
        if (!button->is_pressed) {
            // Đây là một lần nhấn mới được xác nhận
            button->is_pressed = true;

            // Gửi sự kiện 'BUTTON_PRESSED'
            app_event_t event = { .type = EVENT_BUTTON_PRESSED };
            event.data.button.button_id = button_idx;
            sys_event_queue_send(&event); // Gửi từ task của timer, không phải ISR

            // Khởi động timer để phát hiện nhấn giữ lâu
            if (button->long_press_timer) {
                xTimerStart(button->long_press_timer, 0);
            }
        }
    } else { // Nút đã được nhả ra
        if (button->is_pressed) {
            // Đây là một lần nhả nút được xác nhận
            button->is_pressed = false;

            // Dừng timer phát hiện nhấn giữ lâu
            if (button->long_press_timer) {
                xTimerStop(button->long_press_timer, 0);
            }

            // Gửi sự kiện 'BUTTON_RELEASED'
            app_event_t event = { .type = EVENT_BUTTON_RELEASED };
            event.data.button.button_id = button_idx;
            sys_event_queue_send(&event);
        }
    }
}

/**
 * @brief Hàm callback của timer, được gọi khi hết thời gian nhấn giữ lâu.
 */
static void long_press_timer_callback(TimerHandle_t xTimer)
{
    uint32_t button_idx = (uint32_t)pvTimerGetTimerID(xTimer);
    button_control_t *button = &button_config[button_idx];

    // Kiểm tra lại xem nút có còn đang được nhấn không
    if (button->is_pressed) {
        // Gửi sự kiện 'BUTTON_LONG_PRESS'
        app_event_t event = { .type = EVENT_BUTTON_LONG_PRESS };
        event.data.button.button_id = button_idx;
        sys_event_queue_send(&event);
    }
}


/**
 * @brief Trình phục vụ ngắt (ISR) cho sự kiện thay đổi trạng thái GPIO.
 *
 * Hàm này được gọi ngay lập tức khi có sự thay đổi mức logic trên chân GPIO của nút bấm.
 * Nhiệm vụ chính của nó là khởi động timer chống dội.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t button_idx = (uint32_t)arg;
    
    // Khởi động lại (reset) timer chống dội.
    // Chúng ta không kiểm tra trạng thái ở đây để tránh đọc tín hiệu nhiễu.
    // Việc này sẽ được thực hiện trong callback của timer.
    xTimerResetFromISR(button_config[button_idx].debounce_timer, NULL);
}


/*
 * =====================================================================================
 * PHẦN 4: CÁC HÀM API CÔNG KHAI
 * =====================================================================================
 */

/**
 * @brief Khởi tạo các nút bấm, bao gồm cấu hình GPIO và cài đặt ISR.
 * 
 * Đây là hàm nội bộ của HAL, được gọi bởi `board_hal_init()`.
 */
esp_err_t board_button_init(void)
{
    ESP_LOGI(TAG, "Đang khởi tạo nút bấm...");

    // Cấu hình chung cho GPIO ngắt
    gpio_install_isr_service(0);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        button_control_t *button = &button_config[i];

        // Cấu hình chân GPIO
        gpio_config_t io_conf;
        io_conf.intr_type = GPIO_INTR_ANYEDGE;      // Kích hoạt ngắt ở cả sườn lên và sườn xuống
        io_conf.pin_bit_mask = (1ULL << button->gpio_num);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1;                     // Bật điện trở kéo lên nội bộ
        io_conf.pull_down_en = 0;
        gpio_config(&io_conf);

        // Tạo các timer
        // Timer chống dội là one-shot (chỉ chạy 1 lần)
        button->debounce_timer = xTimerCreate("debounce_tmr", pdMS_TO_TICKS(DEBOUNCE_PERIOD_MS), pdFALSE, (void *)i, debounce_timer_callback);
        // Timer nhấn giữ lâu cũng là one-shot
        button->long_press_timer = xTimerCreate("longpress_tmr", pdMS_TO_TICKS(LONG_PRESS_PERIOD_MS), pdFALSE, (void *)i, long_press_timer_callback);

        if (!button->debounce_timer || !button->long_press_timer) {
            ESP_LOGE(TAG, "Không thể tạo timer cho nút bấm %d", i);
            return ESP_FAIL;
        }

        // Thêm ISR cho chân GPIO tương ứng
        gpio_isr_handler_add(button->gpio_num, gpio_isr_handler, (void *)i);
    }
    
    return ESP_OK;
}