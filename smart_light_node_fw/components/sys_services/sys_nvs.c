/*
 * sys_nvs.c - Triển khai dịch vụ lưu trữ NVS
 */

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "sys_nvs.h" // Nhúng file header công khai

#define TAG "SYS_NVS"
#define FLAGS_PARTITION_LABEL "flags"
boot_flag_t boot_flag;
// --- Định nghĩa các hằng số cho NVS ---

// Namespace là một "thư mục" logic để tránh xung đột tên khóa với các component khác.
#define NVS_NAMESPACE "smart_relay"

// Định nghĩa tên các "khóa" (key) để lưu các mẩu dữ liệu khác nhau.
#define KEY_RELAY_STATE "relay_state"
#define KEY_BLE_MESH_INFO "ble_mesh_info"

// Biến static để giữ handle của namespace.
static nvs_handle_t nvs_app_handle;

/*
 * =====================================================================================
 * CÁC HÀM API CÔNG KHAI
 * =====================================================================================
 */

esp_err_t sys_nvs_init(void)
{
    // 1. Khởi tạo phân vùng NVS flash một lần duy nhất cho toàn bộ hệ thống.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Nếu NVS bị lỗi hoặc phiên bản không tương thích, xóa và khởi tạo lại.
        ESP_LOGW(TAG, "NVS partition is corrupted or in new version, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. Mở namespace của ứng dụng để đọc/ghi.
    ESP_LOGI(TAG, "Đang mở NVS namespace: '%s'", NVS_NAMESPACE);
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_app_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể mở NVS namespace: %s", esp_err_to_name(err));
        return err;
    }
    boot_flag = boot_flags_read();

    return ESP_OK;
}

/**
 * @brief Hàm nội bộ để lưu một khối dữ liệu (blob) vào NVS.
 */
static esp_err_t save_blob(const char *key, const void *data, size_t length)
{
    esp_err_t err = nvs_set_blob(nvs_app_handle, key, data, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể lưu blob cho khóa '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    // Commit thay đổi vào flash.
    err = nvs_commit(nvs_app_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể commit NVS cho khóa '%s': %s", key, esp_err_to_name(err));
    }
    return err;
}

/**
 * @brief Hàm nội bộ để đọc một khối dữ liệu (blob) từ NVS.
 */
static esp_err_t load_blob(const char *key, void *data, size_t *length)
{
    esp_err_t err = nvs_get_blob(nvs_app_handle, key, data, length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Không tìm thấy khóa '%s' trong NVS.", key);
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể đọc blob cho khóa '%s': %s", key, esp_err_to_name(err));
    }
    return err;
}


esp_err_t sys_nvs_save_relay_state(const relay_state_t *state)
{
    ESP_LOGD(TAG, "Đang lưu trạng thái relay: %d", state->onoff);
    return save_blob(KEY_RELAY_STATE, state, sizeof(relay_state_t));
}

esp_err_t sys_nvs_load_relay_state(relay_state_t *state)
{
    size_t length = sizeof(relay_state_t);
    return load_blob(KEY_RELAY_STATE, state, &length);
}

esp_err_t sys_nvs_save_ble_mesh_info(const ble_mesh_info_t *info)
{
    ESP_LOGI(TAG, "Đang lưu thông tin BLE Mesh...");
    return save_blob(KEY_BLE_MESH_INFO, info, sizeof(ble_mesh_info_t));
}

esp_err_t sys_nvs_load_ble_mesh_info(ble_mesh_info_t *info)
{
    size_t length = sizeof(ble_mesh_info_t);
    return load_blob(KEY_BLE_MESH_INFO, info, &length);
}

esp_err_t sys_nvs_erase_key(const char *key)
{
    ESP_LOGW(TAG, "Đang xóa khóa '%s'...", key);
    esp_err_t err = nvs_erase_key(nvs_app_handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể xóa khóa '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    return nvs_commit(nvs_app_handle);
}

esp_err_t sys_nvs_erase_all(void)
{
    ESP_LOGW(TAG, "Đang xóa toàn bộ namespace '%s'...", NVS_NAMESPACE);
    esp_err_t err = nvs_erase_all(nvs_app_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể xóa namespace: %s", esp_err_to_name(err));
        return err;
    }
    return nvs_commit(nvs_app_handle);
}

static const esp_partition_t* find_flags_partition(void) {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 
        ESP_PARTITION_SUBTYPE_DATA_OTA, 
        FLAGS_PARTITION_LABEL
    );

    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found!", FLAGS_PARTITION_LABEL);
        return NULL;
    }

    return partition;
}


bool boot_flags_write(boot_flag_t flag) {
    const esp_partition_t *partition = find_flags_partition();
    if (partition == NULL) {
        return false;
    }

    // Chúng ta cần xóa sector trước khi ghi
    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase flags partition: %s", esp_err_to_name(err));
        return false;
    }
    
    // Ghi giá trị cờ vào đầu phân vùng
    uint8_t flag_value = (uint8_t)flag;
    err = esp_partition_write(partition, 0, &flag_value, sizeof(flag_value));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write flag: %s", esp_err_to_name(err));
        return false;
    }
    boot_flag = flag;
    ESP_LOGI(TAG, "Successfully wrote boot flag: %d", flag_value);
    return true;
}

boot_flag_t boot_flags_read(void) {
    const esp_partition_t *partition = find_flags_partition();
    if (partition == NULL) {
        return BOOT_FLAG_NORMAL_START; // Mặc định
    }

    uint8_t flag_value = 0xFF; // Giá trị mặc định khi flash bị xóa
    esp_err_t err = esp_partition_read(partition, 0, &flag_value, sizeof(flag_value));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read flag: %s", esp_err_to_name(err));
        return BOOT_FLAG_NORMAL_START; // Mặc định
    }
    
    // Nếu giá trị là 0xFF, nghĩa là phân vùng trống, coi như khởi động bình thường
    if (flag_value == 0xFF) {
        return BOOT_FLAG_NORMAL_START;
    }

    ESP_LOGI(TAG, "Read boot flag: %d", flag_value);
    return (boot_flag_t)flag_value;
}