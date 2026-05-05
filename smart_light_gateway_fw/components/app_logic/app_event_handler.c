// components/app_logic/app_event_handler.c

#include "sys_event_queue.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "app_event_handler.h"
#include "sys_nvs.h"
#include "device_control.h"

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

static void decode_device_status(uint8_t *data, uint8_t len, device_status_t *st)
{
    // ESP_LOGI(TAG, "Giải mã device status từ payload, length %d", len);
    // ESP_LOG_BUFFER_HEX("Device Status Data", data, len);
    uint8_t *p = data;
    uint8_t *end = data + len;

    while (p + 2 <= end) // đảm bảo còn type + len
    {
        uint8_t type = *p++;
        uint8_t length = *p++;

        if (p + length > end) break; // tránh overflow

        switch (type)
        {
        case TLV_ONOFF:
            if (length == 1)
                st->onoff = p[0];
            break;

        case TLV_LIGHT_LUX:
            if (length == 2)
                st->light_lux = p[0] | (p[1] << 8);
            break;

        case TLV_CURRENT_MA:
            if (length == 2)
                st->current_ma = p[0] | (p[1] << 8);
            break;
        }

        p += length;
    }
}

void handle_recv_publish_status(uint8_t *data, uint16_t len, uint16_t address)
{
    // ESP_LOGI(TAG, "Nhận được publish message từ 0x%04x, length %d", address, len);
    // ESP_LOG_BUFFER_HEX("Received Publish Data", data, len);
    device_status_t status;
    status.onoff = 404;
    status.light_lux = 404;
    status.current_ma = 404;
    decode_device_status(data, len, &status);
    // ESP_LOGI(TAG, "Decoded Device Status - OnOff: %d, Light Lux: %d, Current mA: %d", 
    //          status.onoff, status.light_lux, status.current_ma);
    get_status_response(address, status, CMD_TYPE_MQTT);
    if (status.onoff != 404) {
        send_alert_history_response(HISTORY_TYPE_ONOFF, status.onoff, address, CMD_TYPE_ALERT);
    }
}

