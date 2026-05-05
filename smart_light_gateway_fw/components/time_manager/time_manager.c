#include "time_manager.h"
#include "esp_sntp.h"
#include "esp_log.h"

#include <time.h>
#include <sys/time.h>

static const char *TAG = "TIME_MANAGER";
static bool s_time_is_synced = false;

// --- Callback khi SNTP đồng bộ thành công ---
static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time successfully synchronized");
    s_time_is_synced = true;
}

// --- Khởi tạo SNTP ---
esp_err_t time_manager_init(void) {
    ESP_LOGI(TAG, "Initializing SNTP...");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "time.google.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_interval(3600 * 1000); // 1h
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_init();

    ESP_LOGI(TAG, "SNTP initialization done. Waiting for time sync...");
    return ESP_OK;
}

// --- Kiểm tra trạng thái đồng bộ ---
bool time_manager_is_synced(void) {
    return s_time_is_synced;
}

// --- Lấy timestamp hiện tại ---
time_t time_manager_get_timestamp(void) {
    time_t now;
    time(&now);
    return now;
}