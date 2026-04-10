/*
 * board_led.c - Triển khai logic điều khiển đèn LED
 */

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "board_hal.h" // Nhúng file header công khai của chúng ta
#include "sys_nvs.h"

#define TAG "BOARD_HAL_LED"

/*
 * =====================================================================================
 * PHẦN 1: CÁC ĐỊNH NGHĨA PHỤ THUỘC PHẦN CỨNG
 * =====================================================================================
 * Đây là phần DUY NHẤT bạn cần chỉnh sửa khi thay đổi bo mạch mới.
 */

// Định nghĩa chu kỳ nhấp nháy (tính bằng mili-giây)
#define BLINK_FAST_PERIOD_MS    100
#define BLINK_SLOW_PERIOD_MS    500

/*
 * =====================================================================================
 * PHẦN 2: CẤU TRÚC DỮ LIỆU NỘI BỘ VÀ BIẾN STATIC
 * =====================================================================================
 * Các thành phần này là riêng tư, chỉ sử dụng trong file này.
 */

// Cấu trúc nội bộ để quản lý trạng thái và tài nguyên của mỗi đèn LED
typedef struct {
    board_led_id_t      id;                 // Định danh chức năng của LED
    gpio_num_t          gpio_num;           // Chân GPIO được sử dụng
    TimerHandle_t       timer_handle;       // Handle của timer dùng cho việc nhấp nháy
    board_led_state_t   current_state;      // Trạng thái hiện tại của LED (được yêu cầu bởi ứng dụng)
    bool                timer_toggle_state; // Trạng thái bật/tắt vật lý khi timer chạy
} led_control_t;

// Mảng để lưu trữ cấu hình và trạng thái cho tất cả các đèn LED của chúng ta.
// Thiết kế này giúp code dễ mở rộng. Thêm một LED mới bằng cách thêm một mục ở đây.
static led_control_t led_config[BOARD_LED_ID_MAX] = {
    [STATUS_LED] = {
        .id = STATUS_LED,
        .gpio_num = STATUS_LED_GPIO,
        .timer_handle = NULL,
        .current_state = LED_STATE_OFF,
        .timer_toggle_state = false,
    },
    [RELAY_STATE_LED] = {
        .id = RELAY_STATE_LED,
        .gpio_num = RELAY_STATE_LED_GPIO,
        .timer_handle = NULL,
        .current_state = LED_STATE_OFF,
        .timer_toggle_state = false,
    },
};

/*
 * =====================================================================================
 * PHẦN 3: CÁC HÀM RIÊNG TƯ (STATIC)
 * =====================================================================================
 */

/**
 * @brief Hàm callback của timer để xử lý việc nhấp nháy LED.
 *
 * Hàm này được gọi bởi dịch vụ timer của FreeRTOS. Nó sẽ đảo trạng thái
 * mức logic của chân GPIO tương ứng với đèn LED.
 *
 * @param xTimer Handle của timer đã hết hạn.
 */
static void led_timer_callback(TimerHandle_t xTimer)
{
    // Tìm xem timer này thuộc về đèn LED nào
    for (int i = 0; i < BOARD_LED_ID_MAX; i++) {
        if (led_config[i].timer_handle == xTimer) {
            // Đảo trạng thái mức logic của GPIO
            led_config[i].timer_toggle_state = !led_config[i].timer_toggle_state;
            gpio_set_level(led_config[i].gpio_num, led_config[i].timer_toggle_state);
            return;
        }
    }
}

/**
 * @brief Khởi tạo một đèn LED duy nhất.
 *
 * Cấu hình GPIO, tạo ra software timer tương ứng.
 *
 * @param led Con trỏ tới cấu trúc điều khiển của đèn LED.
 */
static void initialize_single_led(led_control_t *led)
{
    // Cấu hình GPIO
    gpio_reset_pin(led->gpio_num);
    gpio_set_direction(led->gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(led->gpio_num, 0); // Bắt đầu với trạng thái tắt

    // Tạo một software timer để xử lý việc nhấp nháy cho đèn LED này
    // Chúng ta tạo nó ở đây nhưng chưa khởi chạy.
    led->timer_handle = xTimerCreate("led_timer", pdMS_TO_TICKS(1000), pdTRUE, (void *)led->id, led_timer_callback);
    if (led->timer_handle == NULL) {
        ESP_LOGE(TAG, "Không thể tạo timer cho LED %d", led->id);
    }
}


/*
 * =====================================================================================
 * PHẦN 4: CÁC HÀM API CÔNG KHAI (được định nghĩa trong board_hal.h)
 * =====================================================================================
 */

boot_flag_t boot_flag_fake;
// Hàm này được định nghĩa trong board_hal.h, nhưng chúng ta có thể có một hàm
// trợ giúp cục bộ cho việc khởi tạo riêng phần LED.
esp_err_t board_led_init(void)
{
    ESP_LOGI(TAG, "Đang khởi tạo các đèn LED của bo mạch...");
    for (int i = 0; i < BOARD_LED_ID_MAX; i++) {
        initialize_single_led(&led_config[i]);
    }
    boot_flag_fake = 99;
    return ESP_OK;
}

void boot_flag_change_process()
{
    if (boot_flag != boot_flag_fake) {
        boot_flag_fake = boot_flag;
        switch (boot_flag) {
            case BOOT_FLAG_ADDING_DEVICE_MANUAL:
                ESP_LOGI(TAG, "BOOT FLAG: Thêm thiết bị thủ công");
                board_led_set_state(STATUS_LED, LED_STATE_BLINK_FAST);
                break;
            case BOOT_FLAG_ADDED_DEVICE:
                ESP_LOGI(TAG, "BOOT FLAG: Đã thêm thiết bị");
                board_led_set_state(STATUS_LED, LED_STATE_ON);
                break;
            case BOOT_FLAG_NORMAL_START:
                ESP_LOGI(TAG, "BOOT FLAG: Khởi động bình thường");
                board_led_set_state(STATUS_LED, LED_STATE_BLINK_SLOW);
                break;
            default:
                break;
        }
    }
}

// Hàm công khai để thiết lập trạng thái LED
esp_err_t board_led_set_state(board_led_id_t led_id, board_led_state_t state)
{
    if (led_id >= BOARD_LED_ID_MAX) {
        ESP_LOGE(TAG, "ID đèn LED không hợp lệ: %d", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    led_control_t *led = &led_config[led_id];

    if (led->current_state == state) {
        // Không có gì thay đổi, không cần làm gì thêm
        return ESP_OK;
    }

    // Dừng bất kỳ trạng thái nhấp nháy nào trước đó
    if (led->timer_handle != NULL && xTimerIsTimerActive(led->timer_handle)) {
        xTimerStop(led->timer_handle, portMAX_DELAY);
    }

    // Cập nhật trạng thái mới
    led->current_state = state;
    ESP_LOGI(TAG, "Đặt trạng thái LED %d thành %d", led_id, state);

    switch (state) {
        case LED_STATE_OFF:
            gpio_set_level(led->gpio_num, 0);
            break;

        case LED_STATE_ON:
            gpio_set_level(led->gpio_num, 1);
            break;

        case LED_STATE_BLINK_FAST:
            if (led->timer_handle) {
                xTimerChangePeriod(led->timer_handle, pdMS_TO_TICKS(BLINK_FAST_PERIOD_MS), portMAX_DELAY);
                xTimerStart(led->timer_handle, portMAX_DELAY);
            }
            break;

        case LED_STATE_BLINK_SLOW:
            if (led->timer_handle) {
                xTimerChangePeriod(led->timer_handle, pdMS_TO_TICKS(BLINK_SLOW_PERIOD_MS), portMAX_DELAY);
                xTimerStart(led->timer_handle, portMAX_DELAY);
            }
            break;

        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

//hàm thay dổi trạng thái led relay theo trạng thái relay
esp_err_t board_led_change_relay_state(void)
{
    if(led_config[RELAY_STATE_LED].current_state == LED_STATE_ON) {
        return board_led_set_state(RELAY_STATE_LED, LED_STATE_OFF);
    } else {
        return board_led_set_state(RELAY_STATE_LED, LED_STATE_ON);
    }
}