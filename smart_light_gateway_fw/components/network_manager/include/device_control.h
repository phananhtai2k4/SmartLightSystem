#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>
#include "sys_nvs.h"

typedef enum {
    CMD_TYPE_UDP = 0,
    CMD_TYPE_MQTT = 1,
    CMD_TYPE_MQTT_EPLINK = 2,
    CMD_TYPE_BLUFI = 3,
    CMD_TYPE_BLE = 4,
} cmd_type_t;

#define CMD_SET_DEVICE_CONFIG    "CmdSetDeviceConfig"
#define CMD_GET_DEVICE_ID        "CmdGetDeviceID"
#define CMD_GET_DEVICE_STATUS    "CmdGetStatus"
#define CMD_GET_DATA             "CmdGetData"
#define CMD_SET_DATA             "CmdSetData"
#define CMD_RESET                "CmdReset"
#define CMD_SET_CHANGE_MODE      "CmdSetChangeMode"
#define CMD_GET_WIFI_INFO        "CmdGetWifiInfo"

/**
 * @brief Khởi tạo module điều khiển thiết bị.
 * 
 * Module này sẽ đăng ký vào topic command để nhận lệnh từ server.
 *
 * @return esp_err_t ESP_OK nếu thành công.
 */
esp_err_t device_control_init(void);
int device_control_cmd_handle(const char* msg, cmd_type_t msgType);
void send_response(const char *response, cmd_type_t type);
void get_data_response(uint16_t address, uint8_t state, int devT, cmd_type_t type);
#endif