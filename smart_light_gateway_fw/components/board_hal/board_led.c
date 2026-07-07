#include "board_led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_nvs.h"
#include "mqtt_service.h"
#include "device_control.h"

static const char *TAG = "LED_SERVICE";
static uint8_t flag_last_noti_join_mesh = 0;
// LED timing constants (in multiples of update calls)
#define BLINK_FAST_PERIOD   10   // 100ms (với update mỗi 10ms)
#define BLINK_SLOW_PERIOD   50   // 500ms (với update mỗi 10ms)

// LED control structure
static led_control_t g_led_control = {
    .operation = LED_OP_OFF,
    .blink_period_ms = 0,
    .enabled = true
};

// Internal state variables
static uint16_t g_led_timer = 0;
static bool g_led_state = false;
static boot_flag_t boot_flag_fake;

/**
 * @brief Set LED physical state
 */
static inline void led_set_physical_state(bool on)
{
    gpio_set_level(LED_GPIO, on ? 1 : 0);
    g_led_state = on;
}

/**
 * @brief Set LED mode based on boot flag
 */
void led_service_set_boot_flag(boot_flag_t flag)
{
    boot_flag = flag;
    g_led_timer = 0;
    g_led_state = false;
    
    switch (flag) {
        case BOOT_FLAG_ADDING_GATEWAY_MANUAL:
            // Sáng, nhấp nháy nhanh
            g_led_control.operation = LED_OP_BLINK_FAST;
            g_led_control.blink_period_ms = BLINK_FAST_PERIOD;
            ESP_LOGI(TAG, "LED mode: Adding Gateway Manual (Fast Blink)");
            break;
            
        case BOOT_FLAG_ADDED_GATEWAY:
            // Sáng, không nháy
            g_led_control.operation = LED_OP_ON;
            g_led_control.blink_period_ms = 0;
            ESP_LOGI(TAG, "LED mode: Added Gateway (Solid ON)");
            break;
            
        case BOOT_FLAG_NORMAL_START:
            // Sáng, nhấp nháy chậm
            g_led_control.operation = LED_OP_BLINK_SLOW;
            g_led_control.blink_period_ms = BLINK_SLOW_PERIOD;
            ESP_LOGI(TAG, "LED mode: Normal Start (Slow Blink)");
            break;
            
        default:
            g_led_control.operation = LED_OP_OFF;
            ESP_LOGW(TAG, "Unknown boot flag, LED OFF");
            break;
    }
}

/**
 * @brief Manual LED control
 */
void led_service_set_manual(led_operation_t operation, uint16_t blink_period_ms)
{
    g_led_control.operation = operation;
    g_led_control.blink_period_ms = blink_period_ms;
    g_led_timer = 0;
    g_led_state = false;
    
    ESP_LOGI(TAG, "LED manual mode: %d, period: %d", operation, blink_period_ms);
}

/**
 * @brief Turn off LED
 */
void led_service_off(void)
{
    g_led_control.operation = LED_OP_OFF;
    led_set_physical_state(false);
}

/**
 * @brief Turn on LED continuously
 */
void led_service_on(void)
{
    g_led_control.operation = LED_OP_ON;
    led_set_physical_state(true);
}

/**
 * @brief Update LED state (được gọi định kỳ, ví dụ mỗi 10ms)
 */
void led_service_update(void)
{
    static int count = 0;
    if (!g_led_control.enabled) {
        return;
    }

    if(boot_flag != boot_flag_fake) {
        led_service_set_boot_flag(boot_flag);
        boot_flag_fake = boot_flag;
    }

    if (mqtt_connected && boot_flag == BOOT_FLAG_ADDING_GATEWAY_MANUAL) {
        flag_last_noti_join_mesh = 1;
        count++;
        if (count >= 100) { // Mỗi 1 giây (với update mỗi 10ms)
            count = 0;
            send_get_device_id();
        }
    } else if (flag_last_noti_join_mesh) {
        if(count > 700) {
            flag_last_noti_join_mesh = 0;
            count = 0;
            send_alert_online_response(true, CMD_TYPE_ALERT);
        }
        count++;
    }
    
    switch (g_led_control.operation) {
        case LED_OP_OFF:
            led_set_physical_state(false);
            g_led_timer = 0;
            break;
            
        case LED_OP_ON:
            led_set_physical_state(true);
            g_led_timer = 0;
            break;
            
        case LED_OP_BLINK_FAST:
        case LED_OP_BLINK_SLOW:
            // Xử lý nhấp nháy
            if (g_led_timer >= g_led_control.blink_period_ms) {
                g_led_timer = 0;
                g_led_state = !g_led_state;
                led_set_physical_state(g_led_state);
            }
            g_led_timer++;
            break;
            
        default:
            led_set_physical_state(false);
            break;
    }
}


/**
 * Task update LED - PHẢI CÓ để LED hoạt động
 */
void led_update_task(void *pvParameter)
{
    boot_flag_fake = boot_flag;
    while (1) {
        // Gọi hàm update mỗi 10ms
        led_service_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


/**
 * @brief Initialize LED GPIO
 */
esp_err_t led_service_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIO");
        return ret;
    }
    
    gpio_set_level(LED_GPIO, 0);
    
    ESP_LOGI(TAG, "LED service initialized on GPIO %d", LED_GPIO);
    
    led_service_set_boot_flag(boot_flag);
    
    xTaskCreate(led_update_task, "led_task", 2048*4, NULL, 5, NULL);
    return ESP_OK;
}