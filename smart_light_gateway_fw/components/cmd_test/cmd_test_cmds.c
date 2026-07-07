// components/cmd_test/cmd_test_cmds.c
#include "cmd_test_cmds.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_err.h"
#include "ble_mesh_core.h"
#include "wifi_manager.h"


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
            wifi_manager_switch_to_ap();
            err = ESP_OK;
            break;
        case 4:
            wifi_manager_switch_to_sta("HO", "678967896789");
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
        .command = "on",
        .help = "Thực thi một lệnh test với mã số được cung cấp.",
        .hint = NULL,
        .func = &_cmd_test_handler_off,
        .argtable = &test_cmd_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void cmd_test_register_cmds(void)
{
    _register_test_cmd();
    _register_test_cmd_on();
    _register_test_cmd_off();
}