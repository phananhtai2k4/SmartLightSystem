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
    // "%hu" là định dạng cho unsigned short (tương đương uint16_t).
    int items_scanned = sscanf(version_string, "%hu.%hu.%hu", 
                               &version_array[0], 
                               &version_array[1], 
                               &version_array[2]);

    // sscanf sẽ trả về số lượng mục đã đọc thành công.
    // Chúng ta mong đợi nó đọc được đúng 3 mục.
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
        snprintf(short_id, sizeof(short_id), "GATEWAY_%02X%02X", mac[4], mac[5]);
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

void ble_mesh_get_dev_uuid_vcx(uint8_t *dev_uuid)
{
    if (dev_uuid == NULL) {
        ESP_LOGE(TAG, "%s, Invalid dev_uuid pointer", __func__);
        return;
    }

    ESP_LOGI(TAG, "Bắt đầu xây dựng UUID...");

    // --- Bước 1: Lấy và xác thực địa chỉ MAC ---
    const uint8_t *mac_addr = app_wifi_get_mac_uint();
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "LỖI NGHIÊM TRỌNG: app_wifi_get_mac_uint() trả về NULL!");
        // Treo hệ thống ở đây để debug dễ hơn
        while(1);
        return;
    }
    ESP_LOGI(TAG, "Lấy địa chỉ MAC thành công:");
    ESP_LOG_BUFFER_HEX(TAG, mac_addr, 6);

    // --- Bước 2: Lấy và xác thực phiên bản phần mềm ---
    const char *sw_version_str = device_get_software_version();
    if (sw_version_str == NULL) {
        ESP_LOGE(TAG, "LỖI NGHIÊM TRỌNG: device_get_software_version() trả về NULL!");
        while(1);
        return;
    }
    ESP_LOGI(TAG, "Lấy chuỗi phiên bản thành công: \"%s\"", sw_version_str);
    
    // --- Bước 3: Phân tích chuỗi phiên bản ---
    uint16_t sw_version_array[3];
    bool parse_ok = parse_version_string(sw_version_str, sw_version_array);
    if (!parse_ok) {
        ESP_LOGE(TAG, "LỖI NGHIÊM TRỌNG: Không thể phân tích chuỗi phiên bản!");
        while(1);
        return;
    }
    ESP_LOGI(TAG, "Phân tích phiên bản thành công: %d.%d.%d", 
             sw_version_array[0], sw_version_array[1], sw_version_array[2]);

    // --- Bước 4: Bắt đầu điền dữ liệu vào UUID ---
    uint16_t offset = 2; // Bỏ qua 2 byte magic

    // Sao chép MAC
    memcpy(dev_uuid + offset, mac_addr, 6);
    offset += 6;

    // Nhúng Device Type
    dev_uuid[offset++] = (uint8_t)(3017 >> 8);
    dev_uuid[offset++] = (uint8_t)(3017 & 0xFF);

    // Nhúng phiên bản
    dev_uuid[offset++] = (uint8_t)(sw_version_array[0] >> 8);
    dev_uuid[offset++] = (uint8_t)(sw_version_array[0] & 0xFF);
    dev_uuid[offset++] = (uint8_t)(sw_version_array[1] >> 8);
    dev_uuid[offset++] = (uint8_t)(sw_version_array[1] & 0xFF);
    dev_uuid[offset++] = (uint8_t)(sw_version_array[2] >> 8);
    dev_uuid[offset++] = (uint8_t)(sw_version_array[2] & 0xFF);

    ESP_LOGI(TAG, "Xây dựng UUID thành công:");
    ESP_LOG_BUFFER_HEX(TAG, dev_uuid, 16);
}