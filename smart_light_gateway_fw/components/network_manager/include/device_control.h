#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "esp_err.h"
#include <stdbool.h>
#include "sys_nvs.h"
#include "ble_mesh_core.h"

typedef enum {
    CMD_TYPE_UDP = 0,
    CMD_TYPE_MQTT = 1,
    CMD_TYPE_MQTT_EPLINK = 2,
    CMD_TYPE_BLUFI = 3,
    CMD_TYPE_BLE = 4,
    CMD_TYPE_ALERT = 5,
} cmd_type_t;

typedef enum {
    HISTORY_TYPE_ONOFF_BY_APP = 0,
    HISTORY_TYPE_ONOFF_BY_BUTTON = 1,
    HISTORY_TYPE_ELECTRIC_LEAKAGE = 2,
} history_type_t;

#define CMD_SET_DEVICE_CONFIG    "CmdSetDeviceConfig"
#define CMD_GET_DEVICE_ID        "CmdGetDeviceID"
#define CMD_GET_DATA             "CmdGetData"
#define CMD_SET_DATA             "CmdSetData"
#define CMD_GET_STATUS           "CmdGetStatus"
#define CMD_GET_WIFI_INFO        "CmdGetWifiInfo"
#define CMD_ALERT_ONLINE         "CmdAlertOnline"
#define CMD_ALERT_NODE_ONLINE    "CmdAlertNodeOnline"
#define CMD_ALERT_HISTORY        "CmdAlertHistory"
#define CMD_ALERT_DATA           "CmdAlertData"

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
void get_status_response(uint16_t address, device_status_t status, cmd_type_t type);
void send_get_device_id();
void send_alert_online_response(bool online, cmd_type_t type);
void send_alert_node_online_response(uint16_t address, bool online, cmd_type_t type);
void send_alert_history_response(history_type_t history_type, uint8_t onoff, uint16_t address, cmd_type_t type);
void send_alert_data_response(uint16_t light_lux, uint16_t current_ma, uint16_t address, cmd_type_t type);
#endif