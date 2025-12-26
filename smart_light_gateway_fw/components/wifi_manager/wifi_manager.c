


// File: components/wifi_manager/wifi_manager.c
//example: 
#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sys_nvs.h"
#include "esp_wifi_types.h"
#include "sys_event_queue.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "device_identity.h"
#include "mqtt_service.h"
#include "bluetooth_init.h"
#include "udp_service.h"

// --- Lấy từ cả hai ví dụ ---
static const char *TAG = "WIFI_MANAGER";
static char *s_ssid = NULL; // Biến toàn cục để lưu SSID
static char *s_pass = NULL; // Biến toàn cục để lưu mật khẩu
#define WIFI_MAXIMUM_RETRY  5 // Giới hạn số lần thử lại
#define WIFI_AP_PASS      ""
/* FreeRTOS event group để báo hiệu kết nối */
static EventGroupHandle_t s_wifi_event_group;
/* Các bit sự kiện */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

esp_err_t wifi_manager_switch_to_ap(void)
{
    ESP_LOGI(TAG, "Switching to AP mode...");
    mqtt_service_stop_and_deinit();
    bluetooth_deinit();
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*)wifi_config.ap.ssid, device_get_short_id(), sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
	strncpy((char*)wifi_config.ap.password,  WIFI_AP_PASS, sizeof(wifi_config.ap.password));
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP started. SSID:%s", device_get_short_id());
    ESP_LOGI(TAG, "Starting UDP service...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    udp_service_start();
    return ESP_OK;
}

esp_err_t wifi_manager_switch_to_sta(const char *ssid, const char *pass)
{
    ESP_LOGI(TAG, "Switching to STA mode and connecting to SSID: %s", ssid);
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "STA started and connecting...");
    return ESP_OK;
}
// Event handler này bây giờ chỉ làm nhiệm vụ log và thử kết nối lại.
// Nó không cần event group nữa vì trạng thái sẽ được quản lý bởi các module cấp cao hơn (như Blufi).
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to the AP after %d retries.", WIFI_MAXIMUM_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        ESP_ERROR_CHECK(nvs_storage_write_wifi_creds(s_ssid, s_pass));
        ESP_LOGI(TAG, "Mạng đã sẵn sàng (qua Wi-Fi).");
        app_event_t app_evt = { .type = EVENT_NETWORK_CONNECTED };
        sys_event_queue_send(&app_evt);
    } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

esp_err_t wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi Manager...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
	esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group!");
        return ESP_FAIL;
    }
    // Đăng ký event handler mặc định cho Wi-Fi
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_LOGI(TAG, "Wi-Fi manager initialized.");
    return ESP_OK;
}

// Hàm này giờ sẽ non-blocking. Nó chỉ cấu hình và bắt đầu quá trình kết nối.
esp_err_t wifi_manager_connect_start(const char *ssid, const char *pass)
{
    s_ssid = (char *)ssid; // Lưu SSID vào biến toàn cục
    s_pass = (char *)pass; // Lưu mật khẩu vào biến toàn cục
    s_retry_num = 0;
    // esp_wifi_disconnect();

    wifi_config_t wifi_config = {0};
    // wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Setting Wi-Fi configuration SSID [%s]...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	esp_wifi_set_max_tx_power(80);
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

void wifi_manager_disconnect(void)
{
    esp_wifi_disconnect();
}
esp_err_t wifi_manager_wait_for_ip(void)
{
    // Đợi cho đến khi một trong hai bit WIFI_CONNECTED_BIT hoặc WIFI_FAIL_BIT được set
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, // Không xóa bit sau khi đọc
            pdFALSE, // Đợi một trong hai bit là được
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP successfully!");
        ESP_ERROR_CHECK(nvs_storage_write_wifi_creds(s_ssid, s_pass));
        ESP_LOGI(TAG, "Mạng đã sẵn sàng (qua Wi-Fi).");
        app_event_t app_evt = { .type = EVENT_NETWORK_CONNECTED };
        sys_event_queue_send(&app_evt);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP!");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}