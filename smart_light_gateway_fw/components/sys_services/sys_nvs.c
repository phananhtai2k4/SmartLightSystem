// File: components/sys_services/sys_nvs.c

#include "sys_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h> // Cần cho memset
#include "esp_partition.h"
// #include "topic_logic.h" // Để lấy cấu hình thiết bị


static const char *TAG = "NVS_STORAGE";
#define FLAGS_PARTITION_LABEL "flags"
device_config_t g_device_config;
boot_flag_t boot_flag;
// --- Các định nghĩa cho Wi-Fi ---
#define WIFI_NAMESPACE "wifi_config"
#define KEY_WIFI_SSID  "ssid"
#define KEY_WIFI_PASS  "password"

// --- CÁC ĐỊNH NGHĨA MỚI CHO MQTT ---
#define MQTT_NAMESPACE "mqtt_config"
#define KEY_MQTT_BROKER "broker_url"
#define KEY_MQTT_USER   "username"
#define KEY_MQTT_PASS   "password"
#define KEY_MQTT_SUB    "topic_sub"
#define KEY_MQTT_PUB    "topic_pub"
#define KEY_MQTT_ALERT  "topic_alert"
#define LED_STATE_NAMESPACE "led_states"
#define SCHEDULE_NAMESPACE "schedules"
#define SCHEDULE_KEY "schedule_array"
// Mảng chứa các key trong NVS tương ứng với led_id_t
static const char *NVS_LED_KEYS[LED_ID_MAX] = {
    [LED_ID_1] = "led1_state",
    [LED_ID_2] = "led2_state",
    [LED_ID_3] = "led3_state",
};

#define DEV_CONFIG_NAMESPACE "dev_config_ns"  // Namespace trong NVS
#define DEV_CONFIG_KEY       "device_config"  // Key để lưu blob cấu hình

// Namespace là một "thư mục" logic để tránh xung đột tên khóa với các component khác.
#define NVS_NAMESPACE "smart_relay"

// Định nghĩa tên các "khóa" (key) để lưu các mẩu dữ liệu khác nhau.
#define KEY_RELAY_STATE "relay_state"
#define KEY_BLE_MESH_INFO "ble_mesh_info"

// Biến static để giữ handle của namespace.
static nvs_handle_t nvs_app_handle;

// Hàm khởi tạo, lấy trực tiếp từ ví dụ
esp_err_t nvs_storage_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing and re-initializing...");
        // Xóa phân vùng NVS và thử lại
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "NVS initialized successfully.");
    }
    boot_flag = boot_flags_read();
    return err;
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

// Hàm ghi, lấy cảm hứng từ ví dụ
esp_err_t nvs_storage_write_wifi_creds(const char *ssid, const char *pass)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. Mở NVS handle
    err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for writing!", esp_err_to_name(err));
        return err;
    }

    // 2. Ghi chuỗi SSID
    err = nvs_set_str(my_handle, KEY_WIFI_SSID, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write SSID to NVS: %s", esp_err_to_name(err));
        goto cleanup; // Nhảy đến phần dọn dẹp nếu có lỗi
    }

    // 3. Ghi chuỗi Password
    err = nvs_set_str(my_handle, KEY_WIFI_PASS, pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write Password to NVS: %s", esp_err_to_name(err));
        goto cleanup;
    }

    // 4. Lưu các thay đổi vào flash
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes to NVS: %s", esp_err_to_name(err));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Wi-Fi credentials written to NVS successfully.");

// Nhãn cleanup để đảm bảo handle luôn được đóng
cleanup:
    nvs_close(my_handle);
    return err;
}


// Hàm đọc, lấy cảm hứng từ ví dụ
esp_err_t nvs_storage_read_wifi_creds(char *ssid, size_t ssid_len, char *pass, size_t pass_len)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. Mở NVS handle
    err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for reading!", esp_err_to_name(err));
        return err;
    }

    // 2. Đọc chuỗi SSID
    // Lưu ý: nvs_get_str yêu cầu size của buffer phải được truyền vào dưới dạng con trỏ
    err = nvs_get_str(my_handle, KEY_WIFI_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "SSID not found in NVS.");
        } else {
            ESP_LOGE(TAG, "Failed to read SSID from NVS: %s", esp_err_to_name(err));
        }
        goto cleanup;
    }

    // 3. Đọc chuỗi Password
    err = nvs_get_str(my_handle, KEY_WIFI_PASS, pass, &pass_len);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "Password not found in NVS.");
        } else {
            ESP_LOGE(TAG, "Failed to read Password from NVS: %s", esp_err_to_name(err));
        }
        goto cleanup;
    }

    ESP_LOGI(TAG, "Wi-Fi credentials read from NVS successfully.");

cleanup:
    nvs_close(my_handle);
    return err;
}

// <<< HÀM MỚI ĐỂ XÓA THÔNG TIN WI-FI >>>
esp_err_t nvs_storage_erase_wifi_creds(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. Mở NVS handle ở chế độ READWRITE
    err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for erasing!", esp_err_to_name(err));
        return err;
    }

    // 2. Xóa key SSID.
    // Dùng một biến tạm để không ảnh hưởng đến err chính nếu key không tồn tại.
    esp_err_t erase_err = nvs_erase_key(my_handle, KEY_WIFI_SSID);
    if (erase_err != ESP_OK && erase_err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase SSID key: %s", esp_err_to_name(erase_err));
        err = erase_err; // Chỉ gán lỗi nếu nó không phải là 'NOT_FOUND'
        goto cleanup;
    }

    // 3. Xóa key Password
    erase_err = nvs_erase_key(my_handle, KEY_WIFI_PASS);
    if (erase_err != ESP_OK && erase_err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to erase Password key: %s", esp_err_to_name(erase_err));
        err = erase_err;
        goto cleanup;
    }

    // 4. Lưu các thay đổi (việc xóa) vào flash
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase to NVS: %s", esp_err_to_name(err));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Wi-Fi credentials erased from NVS successfully.");

cleanup:
    nvs_close(my_handle);
    // Nếu không có lỗi nào nghiêm trọng xảy ra, trả về ESP_OK
    // Ngay cả khi các key không tồn tại để xóa, ta vẫn coi là thành công.
    return (err == ESP_ERR_NVS_NOT_FOUND) ? ESP_OK : err;
}
/**
 * @brief Ghi toàn bộ cấu hình MQTT vào NVS.
 */
esp_err_t nvs_storage_write_mqtt_config(const mqtt_config_t *config)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(MQTT_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for writing MQTT!", esp_err_to_name(err));
        return err;
    }

    // Ghi từng trường của cấu trúc vào NVS
    err = nvs_set_str(my_handle, KEY_MQTT_BROKER, config->broker_url);
    if (err != ESP_OK) goto mqtt_cleanup;

    err = nvs_set_str(my_handle, KEY_MQTT_USER, config->username);
    if (err != ESP_OK) goto mqtt_cleanup;

    err = nvs_set_str(my_handle, KEY_MQTT_PASS, config->password);
    if (err != ESP_OK) goto mqtt_cleanup;

    err = nvs_set_str(my_handle, KEY_MQTT_SUB, config->topic_sub);
    if (err != ESP_OK) goto mqtt_cleanup;

    err = nvs_set_str(my_handle, KEY_MQTT_PUB, config->topic_pub);
    if (err != ESP_OK) goto mqtt_cleanup;

    err = nvs_set_str(my_handle, KEY_MQTT_ALERT, config->topic_alert);
    if (err != ESP_OK) goto mqtt_cleanup;

    // Lưu các thay đổi
    err = nvs_commit(my_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MQTT configuration written to NVS successfully.");
    }

mqtt_cleanup:
    if (err != ESP_OK) {
         ESP_LOGE(TAG, "Failed to write MQTT config to NVS: %s", esp_err_to_name(err));
    }
    nvs_close(my_handle);
    return err;
}


/**
 * @brief Đọc toàn bộ cấu hình MQTT từ NVS.
 */
esp_err_t nvs_storage_read_mqtt_config(mqtt_config_t *config)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Xóa bộ nhớ đệm trước khi đọc để đảm bảo không có dữ liệu rác
    memset(config, 0, sizeof(mqtt_config_t));

    err = nvs_open(MQTT_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "MQTT config not found or NVS open error (%s).", esp_err_to_name(err));
        return err;
    }

    // Biến tạm để chứa độ dài buffer
    size_t required_size;

    // Đọc từng trường
    required_size = sizeof(config->broker_url);
    err = nvs_get_str(my_handle, KEY_MQTT_BROKER, config->broker_url, &required_size);
    if (err != ESP_OK) goto mqtt_read_cleanup;

    required_size = sizeof(config->username);
    err = nvs_get_str(my_handle, KEY_MQTT_USER, config->username, &required_size);
    if (err != ESP_OK) goto mqtt_read_cleanup;

    required_size = sizeof(config->password);
    err = nvs_get_str(my_handle, KEY_MQTT_PASS, config->password, &required_size);
    if (err != ESP_OK) goto mqtt_read_cleanup;
    
    required_size = sizeof(config->topic_sub);
    err = nvs_get_str(my_handle, KEY_MQTT_SUB, config->topic_sub, &required_size);
    if (err != ESP_OK) goto mqtt_read_cleanup;
    
    required_size = sizeof(config->topic_pub);
    err = nvs_get_str(my_handle, KEY_MQTT_PUB, config->topic_pub, &required_size);
    if (err != ESP_OK) goto mqtt_read_cleanup;
    
    required_size = sizeof(config->topic_alert);
    err = nvs_get_str(my_handle, KEY_MQTT_ALERT, config->topic_alert, &required_size);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MQTT configuration read from NVS successfully.");
    }

mqtt_read_cleanup:
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to read MQTT config from NVS: %s", esp_err_to_name(err));
    }
    nvs_close(my_handle);
    return err;
}


/**
 * @brief Xóa toàn bộ cấu hình MQTT khỏi NVS.
 */
esp_err_t nvs_storage_clear_mqtt_config(void)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(MQTT_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle for erasing MQTT!", esp_err_to_name(err));
        return err;
    }

    // Xóa toàn bộ namespace, cách này nhanh và gọn hơn xóa từng key
    err = nvs_erase_all(my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase MQTT namespace: %s", esp_err_to_name(err));
        goto mqtt_erase_cleanup;
    }

    // Lưu thay đổi
    err = nvs_commit(my_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MQTT configuration erased from NVS successfully.");
    }

mqtt_erase_cleanup:
    nvs_close(my_handle);
    return err;
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