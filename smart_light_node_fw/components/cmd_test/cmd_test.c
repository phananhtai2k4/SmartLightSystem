// components/cmd_test/cmd_test.c
#include "cmd_test.h"
#include "esp_console.h"
#include "esp_log.h"
#include "cmd_test_cmds.h" // Các lệnh test chuyên dụng

static const char* TAG = "CMD_TEST";

esp_err_t cmd_test_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    
    // Tùy chỉnh dấu nhắc lệnh
    repl_config.prompt = "GW>";
    // Thiết lập độ dài tối đa cho dòng lệnh
    repl_config.max_cmdline_length = 256;
    // Tăng kích thước stack nếu cần cho các lệnh phức tạp
    repl_config.task_stack_size = 4096 * 2; 

    ESP_LOGI(TAG, "Đang khởi tạo giao diện dòng lệnh (CLI) để test...");

    // Đăng ký các bộ lệnh
    esp_console_register_help_command();
    cmd_test_register_cmds();  // Đăng ký các lệnh test

    // // Thiết lập phần cứng giao tiếp (sử dụng UART mặc định)
    // esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    // esp_err_t err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Không thể khởi tạo REPL qua UART: %s", esp_err_to_name(err));
    //     return err;
    // }
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể khởi tạo REPL qua UART: %s", esp_err_to_name(err));
        return err;
    }
    // Bắt đầu vòng lặp REPL. Hàm này sẽ không quay trở lại.
    err = esp_console_start_repl(repl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể bắt đầu REPL: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}