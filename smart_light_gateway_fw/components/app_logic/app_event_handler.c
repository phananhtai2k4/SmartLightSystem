// components/app_logic/app_event_handler.c

#include "sys_event_queue.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "app_event_handler.h"
#include "sys_nvs.h"

#include "mqtt_service.h"

static const char *TAG = "APP_EVENT_HANDLER";

void handle_network_connected(void) {
    mqtt_service_init_and_start();
}

void handle_network_disconnected(void) {
    ESP_LOGI(TAG, "Mạng đã mất kết nối hoàn toàn.");
}

void handle_onoff_set_event(const event_data_onoff_set_t *onoff_data)
{
    ESP_LOGI(TAG, "nhận sự kiện OnOff Set: state=%d từ src=0x%04x", 
             onoff_data->onoff_state, onoff_data->ctx.addr);
}

