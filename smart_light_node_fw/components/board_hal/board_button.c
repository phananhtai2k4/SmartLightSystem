#include "board_hal.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_event_queue.h"
#include "sys_nvs.h"
#include <math.h>
#include "ble_mesh_core.h"

static const char *TAG = "BUTTON";

// Scan period
#define BUTTON_SCAN_PERIOD_MS   10

// Ngưỡng thời gian (tính theo số lần lặp, mỗi lần = 10ms)
#define BUTTON_WAIT_SHORT       5       // Phát hiện bấm ngay
#define BUTTON_WAIT_3S          300     // 3 giây = 300 * 10ms
#define BUTTON_WAIT_5S          500     // 7 giây = 700 * 10ms
// Có thể thêm:
// #define BUTTON_WAIT_5S          500     // 5 giây
// #define BUTTON_WAIT_10S         1000    // 10 giây

// Trạng thái button
static uint8_t button_state_prev = 0;   // Trạng thái trước
static uint8_t button_state_cur = 0;    // Trạng thái hiện tại
static uint16_t button_repeat = 0;      // Đếm số lần lặp

// Flags để track event đã gửi chưa
static bool event_pressed_sent = false;
static bool event_3s_sent = false;
static bool event_5s_sent = false;

static TaskHandle_t button_task_handle = NULL;

/**
 * @brief Đọc và phân tích trạng thái button
 * @return 1 nếu button đang được bấm, 0 nếu không
 */
static uint8_t button_analys(void)
{
    // GPIO 0 là active LOW (bấm = 0, thả = 1)
    int level = gpio_get_level(BUTTON_GPIO);
    return (level == 0) ? 1 : 0;
}

/**
 * @brief Xử lý thay đổi trạng thái button và gửi events
 */
static void button_state_change(uint8_t cur_state, uint16_t repeat)
{
    if (cur_state != 0) {
        if (repeat == BUTTON_WAIT_SHORT) {
            ESP_LOGI(TAG, "Button PRESSED");
            
            // Reset flags
            event_pressed_sent = true;
            event_3s_sent = false;
            event_5s_sent = false;
            board_led_change_relay_state();
            
            // Gửi event pressed
            app_event_t evt = { .type = EVENT_BUTTON_PRESSED };
            sys_event_queue_send(&evt);
        }
        // Mốc 3 giây
        if (repeat == BUTTON_WAIT_3S && !event_3s_sent) {
            ESP_LOGI(TAG, "Button HOLD 3S");
            event_3s_sent = true;
            
            app_event_t evt = { .type = EVENT_BUTTON_HOLD_3S };
            sys_event_queue_send(&evt);
        }
        
        // Mốc 7 giây
        if (repeat == BUTTON_WAIT_5S && !event_5s_sent) {
            ESP_LOGI(TAG, "Button HOLD 7S");
            event_5s_sent = true;
            
            app_event_t evt = { .type = EVENT_BUTTON_HOLD_5S };
            sys_event_queue_send(&evt);
        }
    }
    
    // Phát hiện BUTTON RELEASED (từ 1 -> 0)
    if (cur_state == 0 && button_state_prev != 0) {
        ESP_LOGI(TAG, "Button RELEASED (held for %d x 10ms)", repeat);
        
        // Gửi event released
        app_event_t evt = { .type = EVENT_BUTTON_RELEASED };
        sys_event_queue_send(&evt);
        
        // Reset flags
        event_pressed_sent = false;
        event_3s_sent = false;
        event_5s_sent = false;
    }
}

/**
 * @brief Xử lý button - được gọi mỗi 10ms
 */
static void button_process(void)
{
    button_state_prev = button_state_cur;
    button_state_cur = button_analys();
    
    // Nếu trạng thái giống nhau thì tăng repeat counter
    if (button_state_prev == button_state_cur) {
        button_repeat++;
    } else {
        button_repeat = 0;
    }
    
    // Xử lý thay đổi trạng thái
    button_state_change(button_state_cur, button_repeat);
}

static void send_current_status_process(void)
{
    // Gửi trạng thái hiện tại lên mạng mỗi 30 giây
    static uint32_t counter = 0;
    
    static uint8_t last_sent_onoff = 0;
    static uint16_t last_sent_lux = 0;
    static uint16_t last_sent_current = 0;
    counter++;
    
    // if (counter >= 1000 || ((abs(saved_status.light_lux - last_sent_lux) > 20 || abs(saved_status.current_ma - last_sent_current) > 20) && counter >= 200)) {
    if (counter >= 1000) {
        last_sent_onoff = saved_status.onoff;
        last_sent_lux = saved_status.light_lux;
        last_sent_current = saved_status.current_ma;
        counter = 0;
        ble_mesh_core_publish_current_status();
    }
}

/**
 * @brief Task scan button
 */
static void task_button(void *parm)
{
    ESP_LOGI(TAG, "Button task started");
    
    for (;;) {
        button_process();
        boot_flag_change_process();
        send_current_status_process();
        vTaskDelay(pdMS_TO_TICKS(BUTTON_SCAN_PERIOD_MS));
    }
}

/**
 * @brief Khởi tạo button service
 */
esp_err_t board_button_init(void)
{
    // Cấu hình GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIO");
        return ret;
    }
    
    // Khởi tạo trạng thái
    button_state_prev = 0;
    button_state_cur = 0;
    button_repeat = 0;
    event_pressed_sent = false;
    event_3s_sent = false;
    event_5s_sent = false;
    
    // Tạo task
    BaseType_t task_ret = xTaskCreate(
        task_button,
        "button_task",
        2048*8,
        NULL,
        5,
        &button_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Button service initialized on GPIO %d", BUTTON_GPIO);
    ESP_LOGI(TAG, "Hold thresholds: 3s (300), 7s (700)");
    
    return ESP_OK;
}
