/*
 * board_hal.c - Triển khai hàm khởi tạo tổng thể cho Lớp trừu tượng phần cứng
 *
 * File này gọi các hàm khởi tạo cho từng module phần cứng riêng lẻ như LED,
 * nút bấm, và các ngoại vi khác.
 */

#include "esp_log.h"
#include "board_hal.h" // Nhúng file header công khai
#include "esp_err.h"

#define TAG "BOARD_HAL"

extern esp_err_t board_led_init(void);
extern esp_err_t board_button_init(void);
extern esp_err_t board_adc_init(void);

bool log_adc1 = false;
bool log_adc2 = false;
bool log_adc3 = false;

/**
 * @brief Khởi tạo tất cả các phần cứng đặc thù của bo mạch.
 *
 * Đây là hàm công khai được định nghĩa trong board_hal.h.
 */
esp_err_t board_hal_init(void)
{
    ESP_LOGI(TAG, "Đang khởi tạo Lớp trừu tượng phần cứng (HAL)...");
    esp_err_t err;

    //Khởi tạo module LED
    err = board_led_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo module LED thất bại!");
        return err;
    }

    //Khởi tạo module nút bấm
    err = board_button_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo module nút bấm thất bại!");
        return err;
    }

    //Khởi tạo module ADC
    err = board_adc_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khởi tạo module ADC thất bại!");
        return err;
    }

    // board_adc_rms_init();
    
    ESP_LOGI(TAG, "Khởi tạo HAL thành công.");
    return ESP_OK;
}