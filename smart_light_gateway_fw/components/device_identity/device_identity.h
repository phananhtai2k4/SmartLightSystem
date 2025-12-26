#pragma once

#include <stdint.h>
/**
 * @brief Định danh loại thiết bị
 */
#define DEVICE_TYPE_ID CONFIG_DEVICE_TYPE

/**
 * @brief Lấy MAC dạng chuỗi: "AA:BB:CC:DD:EE:FF"
 * @return con trỏ đến chuỗi MAC
 */
const char* device_get_mac_str(void);

/**
 * @brief Lấy device ID dạng ngắn gọn: "ESP32_AABBCC"
 * @return con trỏ đến chuỗi device ID
 */
const char* device_get_short_id(void);

/**
 * @brief Lấy loại thiết bị từ Kconfig: "sensor_v1", "relay_node"
 * @return con trỏ đến chuỗi loại thiết bị
 */
const char* device_get_type(void);

/**
 * @brief Lấy phiên bản SDK đang sử dụng
 * @return con trỏ đến chuỗi phiên bản SDK
 */
const char* device_get_sdk_version(void);

/**
 * @brief Lấy phiên bản phần mềm hiện tại
 * @return con trỏ đến chuỗi phiên bản phần mềm
 */
const char *device_get_software_version(void);

/**
 * @brief Xây dựng Device UUID tùy chỉnh theo định dạng của công ty.
 * 
 * UUID Format (16 bytes):
 * - Byte 0-1:   Magic bytes (e.g., 0xDD, 0xDD) for company identification.
 * - Byte 2-7:   Device MAC Address (6 bytes) for uniqueness.
 * - Byte 8-9:   Device Type (2 bytes, big-endian).
 * - Byte 10-15: Software Version (6 bytes, represented as 3x uint16_t, big-endian).
 * 
 * @param dev_uuid Con trỏ đến mảng 16 byte để lưu UUID.
 */
void ble_mesh_get_dev_uuid_vcx(uint8_t *dev_uuid);

/**
 * @brief Lấy loại thiết bị dưới dạng số nguyên (int).
 * 
 * Sử dụng để lấy mã sản phẩm từ Kconfig, ví dụ: 3017.
 * 
 * @return int Mã loại thiết bị.
 */
const int device_get_int_type(void);