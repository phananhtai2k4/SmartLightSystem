#include "mqtt_service.h"
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys_nvs.h"
#include "device_control.h" // Để có thể gọi hàm resubscribe_all_topics
#include "freertos/semphr.h"

static const char *TAG = "MQTT_SERVICE";
mqtt_config_t mqtt_config;
typedef struct subscription_node {
    char *topic;
    mqtt_data_callback_t callback;
    int qos;
    struct subscription_node *next;
} subscription_t;

static esp_mqtt_client_handle_t client = NULL;
static subscription_t *subscriptions_head = NULL;
static bool is_connected = false;

// Nhúng các file chứng chỉ
extern const uint8_t server_cert_pem_start[] asm("_binary_hivemq_cloud_ca_crt_start");
extern const uint8_t server_cert_pem_end[]   asm("_binary_hivemq_cloud_ca_crt_end");

static SemaphoreHandle_t g_subscriptions_mutex = NULL;

void resubscribe_all_topics() {
    if (!is_connected) {
        ESP_LOGW(TAG, "Cannot resubscribe, MQTT is not connected.");
        return;
    }
    ESP_LOGI(TAG, "Resubscribing to all topics...");
    device_control_init();
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            is_connected = true;
            nvs_storage_read_mqtt_config(&mqtt_config);
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED, broker URL: %s", mqtt_config.broker_url);
            // Đăng ký các topic trước đó đã lưu trong cấu hình
            resubscribe_all_topics();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            is_connected = false;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA received");
            ESP_LOGI(TAG, "Command received on topic: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
            //Lock mutex trước khi đọc danh sách
            if (xSemaphoreTake(g_subscriptions_mutex, portMAX_DELAY) == pdTRUE) {
                subscription_t *current = subscriptions_head;
                while (current != NULL) {
                    if (strncmp(current->topic, event->topic, event->topic_len) == 0 && strlen(current->topic) == event->topic_len) {
                        if (current->callback) {
                            current->callback(event->topic, event->topic_len, event->data, event->data_len);
                        }
                        break; 
                    }
                    current = current->next;
                }
                //Unlock mutex sau khi đọc xong
                xSemaphoreGive(g_subscriptions_mutex);
            } else {
                ESP_LOGE(TAG, "Failed to take subscription mutex for data handling");
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                 ESP_LOGE(TAG, "Last error reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                 ESP_LOGE(TAG, "Last error reported from tls stack: 0x%x", event->error_handle->esp_tls_stack_err);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_service_init_and_start() {

    strcpy(mqtt_config.broker_url, "mqtts://6abef4b39f794ca5bf9ae3fd100385e6.s1.eu.hivemq.cloud:8883");
    strcpy(mqtt_config.username, "taitumtt04");
    strcpy(mqtt_config.password, "Taitum26102004");
    strcpy(mqtt_config.topic_sub, "smartLight/gateway/");
    strcpy(mqtt_config.topic_pub, "smartLight/app/");
    g_subscriptions_mutex = xSemaphoreCreateMutex();
    if (g_subscriptions_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create subscriptions mutex");
        return ESP_FAIL;
    }
    nvs_storage_write_mqtt_config(&mqtt_config);
    ESP_LOGI(TAG, "Initializing MQTT Service...");
    const esp_mqtt_client_config_t mqtt_cfg = {
        .session.last_will.topic = "/devices/esp32_device_01/status",
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = strlen("offline"),
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
        .broker.address.uri = mqtt_config.broker_url,
        .broker.verification.certificate = (const char *)server_cert_pem_start,
        .credentials = {
            .username = mqtt_config.username,
            .authentication.password = mqtt_config.password,
        }
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        vSemaphoreDelete(g_subscriptions_mutex);
        g_subscriptions_mutex = NULL;
        return ESP_FAIL;
    }else{
        ESP_LOGI(TAG, "Successfully MQTT client");
    }

    esp_err_t err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: 0x%x", err);
        esp_mqtt_client_destroy(client);
        client = NULL;
        vSemaphoreDelete(g_subscriptions_mutex);
        g_subscriptions_mutex = NULL;
        return err;
    }
    
    return esp_mqtt_client_start(client);
}

esp_err_t mqtt_service_subscribe(const char *topic, mqtt_data_callback_t callback, int qos) {
    if (!client) {
        return ESP_ERR_INVALID_STATE;
    }

    subscription_t *new_sub = malloc(sizeof(subscription_t));
    if (!new_sub) {
        return ESP_ERR_NO_MEM;
    }
    new_sub->topic = strdup(topic);
    if (!new_sub->topic) {
        free(new_sub);
        return ESP_ERR_NO_MEM;
    }
    new_sub->callback = callback;
    new_sub->qos = qos;
    //Lock mutex trước khi sửa đổi danh sách
    if (xSemaphoreTake(g_subscriptions_mutex, portMAX_DELAY) == pdTRUE) {
        new_sub->next = subscriptions_head;
        subscriptions_head = new_sub;
        //Unlock mutex
        xSemaphoreGive(g_subscriptions_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take subscription mutex");
        free(new_sub->topic);
        free(new_sub);
        return ESP_FAIL;
    }

    if (is_connected) {
        esp_mqtt_client_subscribe(client, topic, qos);
    }
    ESP_LOGI(TAG, "Queued subscription for topic: %s", topic);
    return ESP_OK;
}

int mqtt_service_publish(const char *topic, const char *data, int len, int qos, int retain) {
    if (!is_connected) {
        ESP_LOGE(TAG, "Cannot publish, MQTT not connected");
        return -1;
    }
    return esp_mqtt_client_publish(client, topic, data, len, qos, retain);
}

// GỢI Ý: Thêm hàm stop và deinit để dọn dẹp tài nguyên
esp_err_t mqtt_service_stop_and_deinit(void) {
    if (!client) {
        return ESP_OK; // Chưa được khởi tạo, không có gì để làm
    }

    ESP_LOGI(TAG, "Stopping and de-initializing MQTT service...");

    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    client = NULL;
    is_connected = false;

    // Giải phóng bộ nhớ của danh sách subscriptions
    if (xSemaphoreTake(g_subscriptions_mutex, portMAX_DELAY) == pdTRUE) {
        subscription_t *current = subscriptions_head;
        while (current != NULL) {
            subscription_t *to_delete = current;
            current = current->next;
            free(to_delete->topic);
            free(to_delete);
        }
        subscriptions_head = NULL;
        xSemaphoreGive(g_subscriptions_mutex);
        
        // Xóa mutex
        vSemaphoreDelete(g_subscriptions_mutex);
        g_subscriptions_mutex = NULL;
    }
    
    return ESP_OK;
}

bool mqtt_service_is_connected(void) {
    return is_connected;
}