#ifndef LED_SERVICE_H
#define LED_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sys_nvs.h"

// LED GPIO Pin
#define LED_GPIO    48

// LED Operation Modes
typedef enum {
    LED_OP_OFF,              // LED tắt
    LED_OP_ON,               // LED sáng liên tục
    LED_OP_BLINK_FAST,       // Nhấp nháy nhanh
    LED_OP_BLINK_SLOW        // Nhấp nháy chậm
} led_operation_t;

// LED Control Structure
typedef struct {
    led_operation_t operation;
    uint16_t blink_period_ms;
    bool enabled;
} led_control_t;

/**
 * @brief Initialize LED service
 * @return ESP_OK on success
 */
esp_err_t led_service_init(void);

/**
 * @brief Set LED mode based on boot flag
 * @param flag Boot flag indicating LED behavior
 */
void led_service_set_boot_flag(boot_flag_t flag);

/**
 * @brief Update LED state (should be called periodically)
 */
void led_service_update(void);

/**
 * @brief Manual LED control (override boot flag mode)
 * @param operation LED operation mode
 * @param blink_period_ms Blink period in milliseconds (if blinking)
 */
void led_service_set_manual(led_operation_t operation, uint16_t blink_period_ms);

/**
 * @brief Turn off LED
 */
void led_service_off(void);

/**
 * @brief Turn on LED continuously
 */
void led_service_on(void);

#endif // LED_SERVICE_H
