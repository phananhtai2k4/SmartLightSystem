#include "device_identity.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_idf_version.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "DEVICE_IDENTITY";
static char mac_str[18] = {0};
static char short_id[32] = {0};
static char sdk_version[32] = {0};
/**
 * @brief Phân tích một chuỗi phiên bản (ví dụ: "2.2.0") thành một mảng 3 số nguyên.
 * 
 * @param version_string Chuỗi phiên bản đầu vào.
 * @param version_array  Mảng uint16_t (kích thước 3) để lưu kết quả.
 * @return true nếu phân tích thành công, false nếu thất bại.
 */
static bool parse_version_string(const char *version_string, uint16_t version_array[3])
{
    if (version_string == NULL || version_array == NULL) {
        return false;
    }

    // Dùng sscanf để đọc 3 số nguyên được phân tách bởi dấu chấm.
    int items_scanned = sscanf(version_string, "%hu.%hu.%hu", 
                               &version_array[0], 
                               &version_array[1], 
                               &version_array[2]);

    // sscanf sẽ trả về số lượng mục đã đọc thành công.
    if (items_scanned == 3) {
        return true;
    }

    // Nếu không thành công, điền giá trị mặc định để tránh lỗi
    ESP_LOGE(TAG, "Failed to parse version string: '%s'", version_string);
    version_array[0] = 0;
    version_array[1] = 0;
    version_array[2] = 0;
    return false;
}
const char* device_get_mac_str(void) {
    if (mac_str[0] == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return mac_str;
}

static uint8_t pu8Mac[6] = {0, 0, 0, 0, 0, 0}; // Biến toàn cục để lưu MAC address
static char strMac[18] = {0}; // Biến toàn cục để lưu chuỗi MAC address
uint8_t* app_wifi_get_mac_uint() {
    if(pu8Mac[0] == 0) {
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(pu8Mac));
        sprintf(strMac,"%02X%02X%02X%02X%02X%02X",pu8Mac[0], pu8Mac[1], pu8Mac[2], pu8Mac[3], pu8Mac[4], pu8Mac[5]);
    }
    ESP_LOGI(TAG, "MAC address: %s", strMac);
    return pu8Mac;
}


const char* device_get_short_id(void) {
    if (short_id[0] == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(short_id, sizeof(short_id), "DEVICE_%02X%02X", mac[4], mac[5]);
    }
    return short_id;
}

const char* device_get_type(void) {
    return DEVICE_TYPE_ID;
}

const int device_get_int_type(void) {
    return atoi(DEVICE_TYPE_ID);
}

const char* device_get_sdk_version(void) {
    if (sdk_version[0] == 0) {
        snprintf(sdk_version, sizeof(sdk_version), IDF_VER);
    }
    return sdk_version;
}

const char *device_get_software_version(void) {
    return CONFIG_SOFTWARE_VERSION;
}