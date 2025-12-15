#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <stdio.h>
#include <string.h>
#include "mqtt_service.h"

static const char *TAG = "DEVICE_CONTROL";

static void command_callback(const char *topic, int topic_len, const char *data, int data_len) {
    ESP_LOGI(TAG, "Command received on topic '%.*s'", topic_len, topic);
    ESP_LOGI(TAG, "Data: %.*s", data_len, data);
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

void device_control_init(void)
{
    xTaskCreate(test_publish_task, "test_publish_task", 4096, NULL, 5, NULL);
    mqtt_service_subscribe(mqtt_config.topic_sub, command_callback, 1);
}
