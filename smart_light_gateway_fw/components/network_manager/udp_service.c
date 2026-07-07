#include "udp_service.h"
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "device_control.h"

static const char *TAG = "UDP_SERVICE";

static TaskHandle_t udp_task_handle = NULL;
static int udp_server_sock = -1;
static bool udp_service_running = false;

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[1024];
    struct sockaddr_in dest_addr;
    
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_SERVICE_PORT);
    
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;
    
    while (udp_service_running) {
        udp_server_sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (udp_server_sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        ESP_LOGI(TAG, "Socket created, binding to port %d", UDP_SERVICE_PORT);
        
        int err = bind(udp_server_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            close(udp_server_sock);
            udp_server_sock = -1;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        ESP_LOGI(TAG, "Socket bound, listening for UDP packets on port %d", UDP_SERVICE_PORT);
        
        // Set timeout để có thể kiểm tra udp_service_running định kỳ
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(udp_server_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
        while (udp_service_running) {
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            
            int len = recvfrom(udp_server_sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                             (struct sockaddr *)&source_addr, &socklen);
            
            if (len < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Timeout, tiếp tục loop để kiểm tra udp_service_running
                    continue;
                }
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            else if (len > 0) {
                // Null-terminate received data
                rx_buffer[len] = 0;
                
                // Get source IP address
                char addr_str[128];
                if (source_addr.ss_family == AF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, 
                               addr_str, sizeof(addr_str) - 1);
                }
                
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "Data: %s", rx_buffer);
                device_control_cmd_handle(rx_buffer, CMD_TYPE_UDP);
            }
        }
        
        if (udp_server_sock != -1) {
            ESP_LOGI(TAG, "Shutting down socket...");
            shutdown(udp_server_sock, 0);
            close(udp_server_sock);
            udp_server_sock = -1;
        }
    }
    
    ESP_LOGI(TAG, "UDP server task stopped");
    udp_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t udp_service_init(void)
{
    ESP_LOGI(TAG, "Initializing UDP service...");
    udp_service_running = false;
    udp_server_sock = -1;
    udp_task_handle = NULL;
    return ESP_OK;
}

esp_err_t udp_service_start(void)
{
    if (udp_service_running) {
        ESP_LOGW(TAG, "UDP service already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    udp_service_running = true;
    
    BaseType_t ret = xTaskCreate(udp_server_task, "udp_server", 4096*2, NULL, 5, &udp_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UDP server task");
        udp_service_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "UDP service started");
    return ESP_OK;
}

esp_err_t udp_service_stop(void)
{
    if (!udp_service_running) {
        ESP_LOGW(TAG, "UDP service not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping UDP service...");
    udp_service_running = false;
    
    // Đợi task kết thúc
    int retry = 0;
    while (udp_task_handle != NULL && retry < 10) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    
    return ESP_OK;
}

esp_err_t udp_service_send_broadcast1(const char *data, size_t len, uint16_t port)
{
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (port == 0) {
        port = UDP_SERVICE_PORT;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket for broadcast: errno %d", errno);
        return ESP_FAIL;
    }
    
    // Enable broadcast
    int broadcast_enable = 1;
    int err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to enable broadcast: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    
    err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error sending broadcast: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Broadcast sent: %d bytes to port %d", len, port);
    close(sock);
    return ESP_OK;
}

esp_err_t udp_service_send_unicast(const char *dest_ip, uint16_t port, const char *data, size_t len)
{
    if (!dest_ip || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket for unicast: errno %d", errno);
        return ESP_FAIL;
    }
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    
    int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error sending unicast to %s:%d: errno %d", dest_ip, port, errno);
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Unicast sent: %d bytes to %s:%d", len, dest_ip, port);
    close(sock);
    return ESP_OK;
}
