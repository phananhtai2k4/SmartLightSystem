/*
 * Continuous ADC read and RMS calculation
 * Custom implementation for smart light node
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "sys_nvs.h"
#include "board_hal.h"
#include "sys_event_queue.h"

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_12
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

// User required read length
#define EXAMPLE_READ_LEN                    4000
#define RMS_CALIBRATION_FACTOR              0.00054317
static adc_channel_t channel[2] = {ADC_CHANNEL_4, ADC_CHANNEL_7}; // ADC_CHANNEL_4 = GPIO32, ADC_CHANNEL_7 = GPIO35
static TaskHandle_t s_task_handle;
static uint32_t last_leakage_time = 0;
TaskHandle_t adc_rms_handle = NULL;
static const char *TAG = "BOARD_ADC_RMS";

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 8192, // Needs to be larger than conv_frame_size
        .conv_frame_size = EXAMPLE_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
    ESP_LOGI(TAG, "Continuous ADC initialized with %d channels, sample freq: %ld Hz", channel_num, dig_cfg.sample_freq_hz);
}

static void continuous_adc_deinit(adc_continuous_handle_t handle)
{
    if (handle) {
        ESP_ERROR_CHECK(adc_continuous_deinit(handle));
    }
}

adc_continuous_handle_t handle = NULL;
volatile bool adc_stop_requested = false;
void board_adc_rms_task(void *pvParameters)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};

    s_task_handle = xTaskGetCurrentTaskHandle();

    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (adc_stop_requested)
        {
            break;
        }
        while (1) {
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) {
                uint32_t num_samples = ret_num / SOC_ADC_DIGI_RESULT_BYTES;
                if (ret_num != EXAMPLE_READ_LEN) {
                    continue;
                }
                
                double ch_max[2] = {0.0, 0.0};
                double ch_min[2] = {4096.0, 4096.0}; // Khởi tạo với giá trị lớn để tìm min đúng
                double ch_sum[2] = {0.0, 0.0};
                uint32_t ch_valid_count[2] = {0, 0};

                // Bước 1: Tính Offset trên TOÀN BỘ mẫu vừa đọc được cho cả 2 kênh
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                    uint8_t ch = EXAMPLE_ADC_GET_CHANNEL(p);
                    int ch_idx = (ch == channel[0]) ? 0 : (ch == channel[1] ? 1 : -1);
                    
                    if (ch_idx >= 0) {
                        double current = (double)EXAMPLE_ADC_GET_DATA(p);
                        ch_sum[ch_idx] += current;
                        ch_valid_count[ch_idx]++;
                        if (current > ch_max[ch_idx]) {
                            ch_max[ch_idx] = current;
                        }
                        if (current < ch_min[ch_idx]) {
                            ch_min[ch_idx] = current;
                        }
                    }
                }

                double ch_voltageOffset[2] = {0.0, 0.0};
                double ch_sq_sum[2] = {0.0, 0.0};

                for (int i = 0; i < 2; i++) {
                    if (ch_valid_count[i] > 0) {
                        ch_voltageOffset[i] = ch_sum[i] / ch_valid_count[i];
                    }
                }

                // Bước 2: Tính RMS cho từng kênh
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                    uint8_t ch = EXAMPLE_ADC_GET_CHANNEL(p);
                    int ch_idx = (ch == channel[0]) ? 0 : (ch == channel[1] ? 1 : -1);
                    
                    if (ch_idx >= 0 && ch_valid_count[ch_idx] > 0) {
                        double diff = (double)EXAMPLE_ADC_GET_DATA(p) - ch_voltageOffset[ch_idx];
                        ch_sq_sum[ch_idx] += diff * diff;
                    }
                }

                bool do_save = false;
                for (int i = 0; i < 2; i++) {
                    if (ch_valid_count[i] > 0) {
                        double rms_raw = sqrt(ch_sq_sum[i] / ch_valid_count[i]);
                        double rms_current = (rms_raw * RMS_CALIBRATION_FACTOR > 0.004) ? (rms_raw * RMS_CALIBRATION_FACTOR) : 0.0;
                        
                        if (i == 0) {
                            saved_status.current_ma = (uint16_t)(rms_current * 1000); // Lưu giá trị RMS của kênh đầu tiên
                        }

                        // In log sau mỗi 500ms
                        static uint32_t last_log_time[2] = {0, 0};
                        if (xTaskGetTickCount() - last_log_time[i] > pdMS_TO_TICKS(500)) {
                            if ((log_adc2 && i == 0) || (log_adc3 && i == 1)) {
                                ESP_LOGI(TAG, "CH%d - ret_num: %ld, Offset: %.2f, RMS Raw: %.2f, Current: %.4f A, Max: %.2f, Min: %.2f, avg: %.2f", 
                                         channel[i], ret_num, ch_voltageOffset[i], rms_raw, rms_current, ch_max[i], ch_min[i], (ch_max[i]+ch_min[i])/2);
                            }
                            last_log_time[i] = xTaskGetTickCount();
                            do_save = true;
                            if (i == 0) {
                                sys_nvs_save_device_status(&saved_status);
                            } else if (i == 1 && saved_status.onoff == 1) { //tôi muốn bị rò trong 3 giây mới gửi event
                                // if (rms_current > 0) {
                                if (rms_current >= 0) {
                                    if (last_leakage_time == 0) {
                                        last_leakage_time = xTaskGetTickCount();
                                    }
                                    if (xTaskGetTickCount() - last_leakage_time > pdMS_TO_TICKS(3000)) {
                                        app_event_t evt = { .type = EVENT_LEAKAGE_CURRENT_DETECTED };
                                        sys_event_queue_send(&evt);
                                    }
                                } else {
                                    last_leakage_time = 0;
                                }
                            }
                        }
                    }
                }
                
                // if (do_save) {
                //     sys_nvs_save_device_status(&saved_status);
                // }
            } else if (ret == ESP_ERR_TIMEOUT) {
                break;
            }
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
    vTaskDelete(NULL);
}
void board_adc_rms_init(void)
{
    ESP_LOGI(TAG, "Creating ADC RMS Task...");
    xTaskCreatePinnedToCore(board_adc_rms_task, "adc_rms_task", 4096 * 2*3, NULL, 5, &adc_rms_handle, 1);
}

void board_adc_rms_deinit(void)
{
    adc_stop_requested = true;

    if (adc_rms_handle)
    {
        xTaskNotifyGive(adc_rms_handle);
    }
}