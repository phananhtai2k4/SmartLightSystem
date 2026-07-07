// components/cmd_test/cmd_test_cmds.c
#include "cmd_test_cmds.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ble_mesh_core.h"
#include "board_hal.h"

static const char *TAG = "CMD_TEST_HANDLERS";

/* =========================================================================
 * Lệnh 'lenh' - Dùng để thực thi các chức năng test dựa trên một mã số
 * =========================================================================*/
static struct {
    struct arg_int *code;
    struct arg_end *end;
} test_cmd_args;

static int _cmd_test_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&test_cmd_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, test_cmd_args.end, argv[0]);
        return 1;
    }
    
    int cmd_code = test_cmd_args.code->ival[0];
    esp_err_t err = ESP_OK;
    switch (cmd_code) {
        case 1:
            err = ble_mesh_core_node_local_reset();
            break;
        case 2:
            err = ble_mesh_core_node_prov_enable();
            break;
        case 3:
            err = ESP_OK;
            break;
        case 4:
            err = ESP_OK;
            break;
        case 5:
            err = ESP_OK;
            break;
        case 6:
            err = ESP_OK;
            break;
        case 7:
            err = ESP_OK;
            break;
        default:
            err = ble_mesh_core_send_onoff_set(cmd_code, 1);
            break;
    }

    if (err != ESP_OK) {
        printf("Lỗi khi thực thi lệnh (mã: %s)\n", esp_err_to_name(err));
        return 1;
    }
    
    printf("Lệnh hoàn tất.\n");
    return 0;
}

static int _cmd_test_handler_on(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&test_cmd_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, test_cmd_args.end, argv[0]);
        return 1;
    }
    
    int cmd_code = test_cmd_args.code->ival[0];
    esp_err_t err = ble_mesh_core_send_onoff_set(cmd_code, 1);

    if (err != ESP_OK) {
        printf("Lỗi khi thực thi lệnh (mã: %s)\n", esp_err_to_name(err));
        return 1;
    }
    
    printf("Lệnh hoàn tất.\n");
    return 0;
}

static int _cmd_test_handler_off(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&test_cmd_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, test_cmd_args.end, argv[0]);
        return 1;
    }
    
    int cmd_code = test_cmd_args.code->ival[0];
    esp_err_t err = ble_mesh_core_send_onoff_set(cmd_code, 0);

    if (err != ESP_OK) {
        printf("Lỗi khi thực thi lệnh (mã: %s)\n", esp_err_to_name(err));
        return 1;
    }
    
    printf("Lệnh hoàn tất.\n");
    return 0;
}

static void _register_test_cmd(void)
{
    test_cmd_args.code = arg_int1(NULL, NULL, "<ma_lenh>", "Mã số để thực thi chức năng test tương ứng");
    test_cmd_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "test",
        .help = "Thực thi một lệnh test với mã số được cung cấp.",
        .hint = NULL,
        .func = &_cmd_test_handler,
        .argtable = &test_cmd_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void _register_test_cmd_on(void)
{
    test_cmd_args.code = arg_int1(NULL, NULL, "<ma_lenh>", "Mã số để thực thi chức năng test tương ứng");
    test_cmd_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "on",
        .help = "Thực thi một lệnh test với mã số được cung cấp.",
        .hint = NULL,
        .func = &_cmd_test_handler_on,
        .argtable = &test_cmd_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void _register_test_cmd_off(void)
{
    test_cmd_args.code = arg_int1(NULL, NULL, "<ma_lenh>", "Mã số để thực thi chức năng test tương ứng");
    test_cmd_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "off",
        .help = "Thực thi một lệnh test tắt với mã số được cung cấp.",
        .hint = NULL,
        .func = &_cmd_test_handler_off,
        .argtable = &test_cmd_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* =========================================================================
 * Lệnh 'log_adc' - Set giá trị bool cho log_adc1, log_adc2, log_adc3
 * =========================================================================*/
static struct {
    struct arg_int *adc_num;
    struct arg_int *state;
    struct arg_end *end;
} log_adc_args;

static int _cmd_log_adc_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&log_adc_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, log_adc_args.end, argv[0]);
        return 1;
    }
    
    int adc_num = log_adc_args.adc_num->ival[0];
    bool state = log_adc_args.state->ival[0] != 0;
    
    switch (adc_num) {
        case 1:
            log_adc1 = state;
            break;
        case 2:
            log_adc2 = state;
            break;
        case 3:
            log_adc3 = state;
            break;
        default:
            printf("Lỗi: ADC number phải là 1, 2, hoặc 3\n");
            return 1;
    }

    printf("Đã set log_adc%d = %s\n", adc_num, state ? "true" : "false");
    return 0;
}

static void _register_log_adc_cmd(void)
{
    log_adc_args.adc_num = arg_int1(NULL, NULL, "<adc_num>", "Số thứ tự ADC (1, 2, 3)");
    log_adc_args.state = arg_int1(NULL, NULL, "<state>", "Trạng thái (0: false, 1: true)");
    log_adc_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "log_adc",
        .help = "Bật/Tắt log cho từng ADC (log_adc <1|2|3> <0|1>)",
        .hint = NULL,
        .func = &_cmd_log_adc_handler,
        .argtable = &log_adc_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

/* =========================================================================
 * Hàm Public: Đăng ký tất cả các lệnh
 * =========================================================================*/
void cmd_test_register_cmds(void)
{
    _register_test_cmd();
    _register_test_cmd_on();
    _register_test_cmd_off();
    _register_log_adc_cmd();
}