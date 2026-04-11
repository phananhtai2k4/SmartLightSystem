/*
 * ble_mesh_core.h - Giao diện công khai cho Lõi BLE Mesh
 *
 * File này định nghĩa các hàm API để các component khác (chủ yếu là app_logic)
 * có thể tương tác với hệ thống BLE Mesh, ví dụ như khởi tạo stack
 * và gửi các tin nhắn.
 */

#ifndef __BLE_MESH_CORE_H__
#define __BLE_MESH_CORE_H__

#include <stdint.h>
#include "esp_err.h"

// Khai báo `extern` cho các hàm callback để `ble_mesh_init.c` có thể thấy chúng
// trong khi chúng được định nghĩa ở `ble_mesh_callbacks.c`.
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
extern esp_ble_mesh_gen_onoff_srv_t onoff_server;
extern void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event, esp_ble_mesh_prov_cb_param_t *param);
extern void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, esp_ble_mesh_cfg_server_cb_param_t *param);
extern void ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event, esp_ble_mesh_generic_server_cb_param_t *param);
extern void ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event, esp_ble_mesh_generic_client_cb_param_t *param);
extern void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event, esp_ble_mesh_model_cb_param_t *param);

#define CID_ESP 0x02E5 // Company ID của Espressif
#define CID_CONTROL 0x01 // Company ID của Control
// Opcode tùy chỉnh cho việc set và status
#define MESH_VND_MODEL_OP_GET_STATE       ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define MESH_VND_MODEL_OP_SET_STATE       ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)
#define MESH_VND_MODEL_OP_SET_STATE_UNACK ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)
#define MESH_VND_MODEL_OP_STATUS          ESP_BLE_MESH_MODEL_OP_3(0x04, CID_ESP)
#define MESH_VND_MODEL_ID_CONTROL_SERVER  0x11
// --- Opcodes cho Control Server ---
#define MESH_VND_OP_SSW_SET_DATA          ESP_BLE_MESH_MODEL_OP_3(0x0F, CID_CONTROL)
#define MESH_VND_OP_SSW_SET_DATA_STATUS   ESP_BLE_MESH_MODEL_OP_3(0x10, CID_CONTROL)
#define MESH_VND_OP_SSW_GET_DATA          ESP_BLE_MESH_MODEL_OP_3(0x11, CID_CONTROL)
#define MESH_VND_OP_SSW_GET_DATA_STATUS   ESP_BLE_MESH_MODEL_OP_3(0x12, CID_CONTROL)

/**
 * @brief Khởi tạo toàn bộ stack và các thành phần của BLE Mesh.
 *
 * Hàm này đăng ký các callbacks, khởi tạo stack với composition data,
 * và bật chế độ cho phép provisioning.
 * Phải được gọi một lần khi ứng dụng khởi động.
 *
 * @return esp_err_t ESP_OK nếu thành công, hoặc mã lỗi nếu thất bại.
 */
esp_err_t ble_mesh_core_init(void);

/**
 * @brief Gửi một tin nhắn Generic OnOff Set (cần xác nhận).
 *
 * Hàm này đóng gói và gửi một lệnh OnOff đến một địa chỉ unicast hoặc group.
 *
 * @param dest_addr Địa chỉ Unicast hoặc Group của thiết bị đích.
 * @param onoff Trạng thái mong muốn (0 = OFF, 1 = ON).
 *
 * @return esp_err_t ESP_OK nếu lệnh gửi được đưa vào hàng đợi thành công.
 */
esp_err_t ble_mesh_core_send_onoff_set(uint16_t dest_addr, uint8_t onoff);

/**
 * @brief Gửi một tin nhắn Generic OnOff Set Unacknowledged (không cần xác nhận).
 *
 * Tương tự như hàm trên nhưng không yêu cầu server phản hồi. Thích hợp
 * cho việc gửi lệnh đến địa chỉ nhóm để giảm lưu lượng mạng.
 *
 * @param dest_addr Địa chỉ Unicast hoặc Group của thiết bị đích.
 * @param onoff Trạng thái mong muốn (0 = OFF, 1 = ON).
 *
 * @return esp_err_t ESP_OK nếu lệnh gửi được đưa vào hàng đợi thành công.
 */
esp_err_t ble_mesh_core_send_onoff_set_unack(uint16_t dest_addr, uint8_t onoff);


/**
 * @brief Publish một tin nhắn OnOff Status.
 *
 * Cho phép node chủ động báo cáo trạng thái của nó đến địa chỉ publish đã được cấu hình.
 * Hữu ích khi trạng thái relay thay đổi do một tác động cục bộ (ví dụ: nút bấm vật lý).
 *
 * @return esp_err_t ESP_OK nếu publish thành công.
 */
esp_err_t ble_mesh_core_publish_onoff_status(void);
esp_err_t ble_mesh_core_node_local_reset(void);
esp_err_t ble_mesh_core_node_prov_enable(void);
void auto_rebind_on_startup(void);
esp_err_t self_bind_vendor_model(uint16_t element_addr, uint16_t app_idx, 
                                        uint16_t model_id, uint16_t company_id);
#endif // __BLE_MESH_CORE_H__