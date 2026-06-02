/*
 * board_adc.c - Doc gia tri ADC 1 kenh theo chu ky 1 giay
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "soc/soc_caps.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "sys_nvs.h"
#include "board_hal.h"

#define TAG "BOARD_ADC"

#define BOARD_ADC_UNIT           ADC_UNIT_2
#define BOARD_ADC_CHANNEL        ADC_CHANNEL_8  // Chân GPIO25
#define BOARD_ADC_ATTEN          ADC_ATTEN_DB_12
#define BOARD_ADC_READ_PERIOD_MS 5000

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_adc_cali_handle = NULL;
static TaskHandle_t s_adc_task_handle = NULL;
static bool s_do_calibration = false;

static bool board_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
	adc_cali_handle_t handle = NULL;
	esp_err_t ret = ESP_FAIL;
	bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	if (!calibrated) {
		adc_cali_curve_fitting_config_t cali_config = {
			.unit_id = unit,
			.chan = channel,
			.atten = atten,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
		if (ret == ESP_OK) {
			calibrated = true;
		}
	}
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	if (!calibrated) {
		adc_cali_line_fitting_config_t cali_config = {
			.unit_id = unit,
			.atten = atten,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
		if (ret == ESP_OK) {
			calibrated = true;
		}
	}
#endif

	*out_handle = handle;
	if (calibrated) {
		ESP_LOGI(TAG, "ADC calibration enabled");
	} else {
		ESP_LOGW(TAG, "ADC calibration unavailable, using raw value");
	}

	return calibrated;
}

static void board_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

static void board_adc_task(void *arg)
{
    (void)arg;

    while (1) {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(s_adc_handle, BOARD_ADC_CHANNEL, &raw);
        if (ret == ESP_OK) {
            // ESP_LOGI(TAG, "ADC raw: %d", raw);

            if (s_do_calibration) {
                int voltage_mv = 0;
                ret = adc_cali_raw_to_voltage(s_adc_cali_handle, raw, &voltage_mv);
                if (ret == ESP_OK) {
                    // ESP_LOGI(TAG, "ADC voltage: %d mV", voltage_mv);

                    // Calculate light intensity in Lux
                    float v_out = (float)voltage_mv / 1000.0;

                    if (v_out > 0.0) {
                        // Calculate LDR resistance (Ohm)
                        float r_ldr = ((3.3 / v_out) - 1.0) * 10000.0;

                        // Convert to kOhm
                        float r_ldr_kOhm = r_ldr / 1000.0;

                        // Calculate Lux (approximate formula for GL5528 LDR)
                        float lux = 500.0 / r_ldr_kOhm;
                        saved_status.light_lux = (uint16_t)lux;
						if (log_adc1) {
							ESP_LOGI(TAG, "LDR resistance: %.2f kOhm | Light intensity: %.2f Lux", r_ldr_kOhm, lux);
						}
                    } else {
                        saved_status.light_lux = 0.0;
                        ESP_LOGI(TAG, "Complete darkness (0 Lux)");
                    }
                    sys_nvs_save_device_status(&saved_status);
                } else {
                    ESP_LOGW(TAG, "Convert raw to voltage failed: %s", esp_err_to_name(ret));
                }
            }
        } else {
            ESP_LOGE(TAG, "Read ADC failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(BOARD_ADC_READ_PERIOD_MS));
    }
}

esp_err_t board_adc_init(void)
{
	if (s_adc_task_handle != NULL) {
		return ESP_OK;
	}

	adc_oneshot_unit_init_cfg_t init_config = {
		.unit_id = BOARD_ADC_UNIT,
	};
	esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Create ADC unit failed: %s", esp_err_to_name(ret));
		return ret;
	}

	adc_oneshot_chan_cfg_t chan_config = {
		.atten = BOARD_ADC_ATTEN,
		.bitwidth = ADC_BITWIDTH_DEFAULT,
	};
	ret = adc_oneshot_config_channel(s_adc_handle, BOARD_ADC_CHANNEL, &chan_config);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Config ADC channel failed: %s", esp_err_to_name(ret));
		ESP_ERROR_CHECK(adc_oneshot_del_unit(s_adc_handle));
		s_adc_handle = NULL;
		return ret;
	}

	s_do_calibration = board_adc_calibration_init(BOARD_ADC_UNIT, BOARD_ADC_CHANNEL, BOARD_ADC_ATTEN, &s_adc_cali_handle);

	BaseType_t task_ret = xTaskCreate(board_adc_task, "board_adc_task", 4096, NULL, 5, &s_adc_task_handle);
	if (task_ret != pdPASS) {
		ESP_LOGE(TAG, "Create ADC task failed");
		if (s_do_calibration) {
			board_adc_calibration_deinit(s_adc_cali_handle);
			s_adc_cali_handle = NULL;
			s_do_calibration = false;
		}
		ESP_ERROR_CHECK(adc_oneshot_del_unit(s_adc_handle));
		s_adc_handle = NULL;
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "ADC initialized: unit=%d channel=%d, period=%dms",
			 BOARD_ADC_UNIT, BOARD_ADC_CHANNEL, BOARD_ADC_READ_PERIOD_MS);
	return ESP_OK;
}
