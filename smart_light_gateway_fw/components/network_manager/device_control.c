
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>
#include "mqtt_service.h"
#include "cJSON.h"
#include "device_control.h"
#include "device_identity.h"
#include "sys_nvs.h"
#include "udp_service.h"
#include "wifi_manager.h"

static const char *TAG = "DEVICE_CONTROL";

// Helper function to send response
void send_response(const char *response, cmd_type_t type) {
    if (type == CMD_TYPE_UDP) {
        udp_service_send_broadcast1(response, strlen(response), 0);
    } else if (type == CMD_TYPE_MQTT) {
        if (mqtt_service_is_connected()) {
            mqtt_service_publish(mqtt_config.topic_pub, response, 0, 1, 0);
        }
    }
}

static int handle_set_change_mode(cJSON *value, cmd_type_t type) {
    ESP_LOGI(TAG, "Handle set change mode");
    // TODO: Implement
    return 0;
}
static int handle_set_device_config(cJSON *value, cmd_type_t type) {
    ESP_LOGI(TAG, "Handle set device config");
    
    if (!value || !cJSON_IsObject(value)) {
        ESP_LOGW(TAG, "Invalid value object");
        return -1;
    }
    
    // Parse các trường từ JSON
    cJSON *wifi_ssid = cJSON_GetObjectItem(value, "wifi_ssid");
    cJSON *wifi_pass = cJSON_GetObjectItem(value, "wifi_pass");
    cJSON *mqtt_user = cJSON_GetObjectItem(value, "mqtt_user");
    cJSON *mqtt_pass = cJSON_GetObjectItem(value, "mqtt_pass");
    cJSON *mqtt_url = cJSON_GetObjectItem(value, "mqtt_url");
    cJSON *mqtt_sub = cJSON_GetObjectItem(value, "mqtt_sub");
    cJSON *mqtt_pub = cJSON_GetObjectItem(value, "mqtt_pub");
    cJSON *mqtt_alert = cJSON_GetObjectItem(value, "mqtt_alert");

    mqtt_config_t config;
    if (cJSON_IsString(mqtt_url)) {
        strncpy(config.broker_url, mqtt_url->valuestring, sizeof(config.broker_url) - 1);
        config.broker_url[sizeof(config.broker_url) - 1] = '\0';
    } else {
        ESP_LOGI(TAG, "Saved MQTT URL: %s", mqtt_url->valuestring);
    }
    if (cJSON_IsString(mqtt_user) && cJSON_IsString(mqtt_pass)) {
        strncpy(config.username, mqtt_user->valuestring, sizeof(config.username) - 1);
        config.username[sizeof(config.username) - 1] = '\0';
        strncpy(config.password, mqtt_pass->valuestring, sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        ESP_LOGI(TAG, "Saved MQTT credentials");
    }
    if (cJSON_IsString(mqtt_sub)) {
        strncpy(config.topic_sub, mqtt_sub->valuestring, sizeof(config.topic_sub) - 1);
        config.topic_sub[sizeof(config.topic_sub) - 1] = '\0';
    }
    if (cJSON_IsString(mqtt_pub)) {
        strncpy(config.topic_pub, mqtt_pub->valuestring, sizeof(config.topic_pub) - 1);
        config.topic_pub[sizeof(config.topic_pub) - 1] = '\0';
    }
    if (cJSON_IsString(mqtt_alert)) {
        strncpy(config.topic_alert, mqtt_alert->valuestring, sizeof(config.topic_alert) - 1);
        config.topic_alert[sizeof(config.topic_alert) - 1] = '\0';
    }
    nvs_storage_write_mqtt_config(&config);
    nvs_storage_write_wifi_creds(wifi_ssid->valuestring, wifi_pass->valuestring);
    // Tạo JSON response với CmdGetData
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "name", CMD_GET_DATA);
    cJSON_AddStringToObject(response, "devT", CONFIG_DEVICE_TYPE);
    cJSON_AddStringToObject(response, "devExtAddr", device_get_mac_str());
    
    // Thêm các thông tin đã lưu vào response
    if (cJSON_IsString(wifi_ssid)) {
        cJSON_AddStringToObject(response, "wifi_ssid", wifi_ssid->valuestring);
    }
    if (cJSON_IsString(wifi_pass)) {
        cJSON_AddStringToObject(response, "wifi_pass", wifi_pass->valuestring);
    }
    if (cJSON_IsString(mqtt_user)) {
        cJSON_AddStringToObject(response, "mqtt_user", mqtt_user->valuestring);
    }
    if (cJSON_IsString(mqtt_pass)) {
        cJSON_AddStringToObject(response, "mqtt_pass", mqtt_pass->valuestring);
    }
    if (cJSON_IsString(mqtt_url)) {
        cJSON_AddStringToObject(response, "mqtt_url", mqtt_url->valuestring);
    }
    if (cJSON_IsString(mqtt_sub)) {
        cJSON_AddStringToObject(response, "mqtt_sub", mqtt_sub->valuestring);
    }
    if (cJSON_IsString(mqtt_pub)) {
        cJSON_AddStringToObject(response, "mqtt_pub", mqtt_pub->valuestring);
    }
    if (cJSON_IsString(mqtt_alert)) {
        cJSON_AddStringToObject(response, "mqtt_alert", mqtt_alert->valuestring);
    }
    
    char *response_str = cJSON_PrintUnformatted(response);
    if (response_str) {
        ESP_LOGI(TAG, "Sending response: %s", response_str);
        send_response(response_str, type);
        cJSON_free(response_str);
    }
    cJSON_Delete(response);
    //resetchip
    ESP_LOGI(TAG, "Device config updated, restarting device...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    esp_restart();
    
    return 0;
}

static int handle_get_device_id(cJSON *value, cmd_type_t type) {
    ESP_LOGI(TAG, "Handle get device id");
    
    // Tạo JSON response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "name", CMD_GET_DEVICE_ID);
    cJSON_AddStringToObject(response, "devT", CONFIG_DEVICE_TYPE);
    cJSON_AddStringToObject(response, "devExtAddr", device_get_mac_str());
    
    char *response_str = cJSON_PrintUnformatted(response);
    if (response_str) {
        ESP_LOGI(TAG, "Sending response: %s", response_str);
        send_response(response_str, type);
        cJSON_free(response_str);
    }
    cJSON_Delete(response);
    
    return 0;
}
static int handle_get_status(cJSON *value, cmd_type_t type) {
    ESP_LOGI(TAG, "Handle get status");
    // TODO: Implement
    return 0;
}
static int handle_reset(cJSON *value, cmd_type_t type) {
    ESP_LOGI(TAG, "Handle reset");
    // TODO: Implement
    return 0;
}

int device_control_cmd_handle(const char* msg, cmd_type_t msgType) {
    ESP_LOGI(TAG, "MSG: %s", msg);
    int ret = -1;
    cJSON *root = cJSON_Parse(msg);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse error");
        return -1;
    }
    if (!cJSON_IsObject(root)) {
        ESP_LOGW(TAG, "Root is not object");
        cJSON_Delete(root);
        return -1;
    }
    cJSON *pJsName = cJSON_GetObjectItem(root, "name");
    if (!cJSON_IsString(pJsName)) {
        ESP_LOGW(TAG, "Missing or invalid 'name'");
        cJSON_Delete(root);
        return -1;
    }
    cJSON *pJsValue = cJSON_GetObjectItem(root, "value");
    if (!cJSON_IsObject(pJsValue)) {
        ESP_LOGW(TAG, "Missing or invalid 'value' object");
    }
    if (0) {}
    else if (strcmp(pJsName->valuestring, CMD_SET_CHANGE_MODE) == 0)
        ret = handle_set_change_mode(pJsValue, msgType);
    else if (strcmp(pJsName->valuestring, CMD_SET_DEVICE_CONFIG) == 0)
        ret = handle_set_device_config(pJsValue, msgType);
    else if (strcmp(pJsName->valuestring, CMD_GET_DEVICE_ID) == 0)
        ret = handle_get_device_id(pJsValue, msgType);
    else if (strcmp(pJsName->valuestring, CMD_GET_DEVICE_STATUS) == 0)
        ret = handle_get_status(pJsValue, msgType);
    else if (strcmp(pJsName->valuestring, CMD_RESET) == 0)
        ret = handle_reset(pJsValue, msgType);
    else {
        ESP_LOGW(TAG, "Command '%s' not supported", pJsName->valuestring);
    }
    cJSON_Delete(root);
    return ret;
}

static void command_callback(const char *topic, int topic_len, const char *data, int data_len) {
    ESP_LOGI(TAG, "Command received on topic '%.*s'", topic_len, topic);
    ESP_LOGI(TAG, "Data: %.*s", data_len, data);
    // Assume all commands are MQTT for now
    device_control_cmd_handle(data, CMD_TYPE_MQTT);
}

static void test_publish_task(void *pvParameters)
{
    const TickType_t delay_30s = pdMS_TO_TICKS(5000);

    ESP_LOGI(TAG, "Relay test task started");

    while (1)
    {
       
        char payload[128];
        strcpy(payload, "hello from device_control!");

        if (mqtt_service_is_connected()) {
            int msg_id = mqtt_service_publish(mqtt_config.topic_pub, payload, 0, 1, 0);
            ESP_LOGI(TAG, "Published msg_id=%d: %s", msg_id, payload);
        } else {
            ESP_LOGW(TAG, "MQTT not connected, skip publish");
        }

        vTaskDelay(delay_30s);
    }
}

esp_err_t device_control_init(void)
{
    // xTaskCreate(test_publish_task, "test_publish_task", 4096, NULL, 5, NULL);
    mqtt_service_subscribe(mqtt_config.topic_sub, command_callback, 1);
    return ESP_OK;
}
