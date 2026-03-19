/*
 * ble_mesh_init.c - Triển khai logic khởi tạo và các hàm API gửi tin nhắn
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_defs.h"
// Nhúng các file header cần thiết
#include "ble_mesh_core.h"
// #include "ble_mesh_composition.h" // Để truy cập biến 'composition'
#include "sys_nvs.h" // Để truy cập thông tin mạng đã lưu
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_bt_device.h"
#include "bluetooth_init.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "device_identity.h"
#define TAG "BLE_MESH_CORE"

// Biến static để theo dõi Transaction ID (TID) cho các tin nhắn gửi đi
static uint8_t tid = 0;
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };
extern esp_ble_mesh_comp_t composition; // Biến này được định nghĩa trong file ble_mesh_composition.c
/*
 * =====================================================================================
 * LOGIC KHỞI TẠO
 * =====================================================================================
 */

// Cấu trúc khai báo thông tin cho quá trình provisioning
static esp_ble_mesh_prov_t provision_info = {
    .uuid = dev_uuid,
#if 0
    .output_size = 4,
    .output_actions = ESP_BLE_MESH_DISPLAY_NUMBER,
    .input_size = 4,
    .input_actions = ESP_BLE_MESH_PUSH,
#else
    .output_size = 0,
    .output_actions = 0,
#endif
};

// static void setup_provisioning_info(void)
// {
//     // Lấy UUID từ địa chỉ MAC để đảm bảo nó là duy nhất
//     // Hàm này được cung cấp bởi ESP-IDF trong các ví dụ, bạn có thể cần tự triển khai
//     // hoặc sao chép từ thư mục common_components của ví dụ.
//     // Tạm thời, chúng ta sẽ giả sử nó đã có.
//     // ble_mesh_get_dev_uuid(dev_uuid); // Uncomment this if you have the helper function

//     provision_info.uuid = dev_uuid;
//     // Tắt OOB để đơn giản hóa quá trình provisioning
//     provision_info.output_size = 0;
//     provision_info.output_actions = 0;
// }

/**
 * @brief Tự động bind lại các model khi khởi động nếu cần.
 */
void auto_rebind_on_startup(void)
{
    // 1. Kiểm tra xem thiết bị đã được provision chưa
    if (!esp_ble_mesh_node_is_provisioned()) {
        ESP_LOGI(TAG, "Thiết bị chưa được provision, bỏ qua tự bind.");
        return;
    }
    ESP_LOGI(TAG, "Thiết bị đã được provision, tiến hành tự bind lại các model.");
    // 3. Thực hiện bind lại
    // Lấy địa chỉ unicast của chính node này
    uint16_t self_addr = esp_ble_mesh_get_primary_element_address();
    uint16_t app_idx = 0; // Giả định AppKey đầu tiên luôn là index 0

    // Bind lại OnOff Client
    esp_ble_mesh_node_bind_app_key_to_local_model(self_addr, 0xFFFF,
                                                ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI, app_idx);
    // Bind lại OnOff Server
    esp_ble_mesh_node_bind_app_key_to_local_model(self_addr, 0xFFFF,
                                            ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, app_idx);
}

esp_err_t ble_mesh_core_init(void)
{
    ESP_LOGI(TAG, "Đang khởi tạo BLE Mesh Core...");
    esp_err_t err;
    ble_mesh_get_dev_uuid(dev_uuid);

    // 1. Cài đặt thông tin provisioning
    // setup_provisioning_info();

    // 2. Đăng ký tất cả các hàm callback
    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(ble_mesh_generic_server_cb);
    esp_ble_mesh_register_generic_client_callback(ble_mesh_generic_client_cb);

    // 3. Khởi tạo BLE Mesh stack với cấu trúc node đã định nghĩa
    // Biến 'composition' được lấy từ file ble_mesh_composition.c
    err = esp_ble_mesh_init(&provision_info, &composition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể khởi tạo mesh stack (err %d)", err);
        return err;
    }
    auto_rebind_on_startup();
    const char *new_device_name = device_get_short_id();
    ESP_LOGI(TAG, "Setting unprovisioned device name to: %s", new_device_name);
    err = esp_ble_mesh_set_unprovisioned_device_name(new_device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi đặt tên thiết bị chưa provision (err %d)", err);
        return err;
    }
    // 4. Bật chế độ cho phép provisioning qua PB-ADV và PB-GATT
    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể bật chế độ provisioning (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node đã được khởi tạo và sẵn sàng để provisioning.");
    return ESP_OK;
}


/*
 * =====================================================================================
 * CÁC HÀM API GỬI TIN NHẮN
 * =====================================================================================
 */

// Hàm nội bộ để điền các tham số chung cho việc gửi tin nhắn client
static void setup_common_client_params(esp_ble_mesh_client_common_param_t *common, uint16_t dest_addr, uint32_t opcode)
{
    // Lấy model handle từ biến 'onoff_client' đã được khai báo trong composition.c
    extern esp_ble_mesh_client_t onoff_client;

    common->opcode = opcode;
    common->model = onoff_client.model; // Dùng client model để gửi
    common->ctx.net_idx = 0; // Luôn dùng net_idx 0 (mặc định)
    common->ctx.app_idx = 0; // Luôn dùng app_idx 0 (mặc định)
    common->ctx.addr = dest_addr;
    common->ctx.send_ttl = 3; // Giá trị TTL hợp lý
    common->msg_timeout = 5000; // Chờ phản hồi trong 5 giây
}

esp_err_t ble_mesh_core_send_onoff_set_unack(uint16_t dest_addr, uint8_t onoff)
{
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set = {0};

    setup_common_client_params(&common, dest_addr, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK);
    
    set.onoff_set.op_en = false;
    set.onoff_set.onoff = onoff;
    set.onoff_set.tid = tid++; // Sử dụng và tăng TID

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể gửi lệnh OnOff Set Unack (err %d)", err);
    }
    return err;
}


esp_err_t ble_mesh_core_send_onoff_set(uint16_t dest_addr, uint8_t onoff)
{
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set = {0};

    setup_common_client_params(&common, dest_addr, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET);
    
    set.onoff_set.op_en = false;
    set.onoff_set.onoff = onoff;
    set.onoff_set.tid = tid++;

    esp_err_t err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể gửi lệnh OnOff Set (err %d)", err);
    }
    return err;
}

esp_err_t ble_mesh_core_publish_onoff_status(void)
{
    // Lấy model handle từ biến 'onoff_server' đã được khai báo trong composition.c
    extern esp_ble_mesh_gen_onoff_srv_t onoff_server;

    // Trạng thái onoff được lưu trong srv->state.onoff
    uint8_t current_state = onoff_server.state.onoff;

    // Publish tin nhắn status
    esp_err_t err = esp_ble_mesh_model_publish(onoff_server.model, 
                                               ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                               sizeof(current_state), 
                                               &current_state, 
                                               ROLE_NODE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể publish OnOff Status (err %d)", err);
    } else {
        ESP_LOGI(TAG, "Đã publish OnOff Status: %d", current_state);
    }

    return err;
}

// esp_err_t esp_ble_mesh_node_local_reset(void)
esp_err_t ble_mesh_core_node_local_reset(void)
{
    ESP_LOGI(TAG, "Đang thực hiện local reset cho BLE Mesh Node...");
    esp_err_t err = esp_ble_mesh_node_local_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Local reset thất bại (err %d)", err);
    } else {
        ESP_LOGI(TAG, "Local reset thành công. Node sẽ cần được provision lại.");
    }
    return err;
}

esp_err_t ble_mesh_core_node_prov_enable(void)
{
    ESP_LOGI(TAG, "Đang bật chế độ provisioning cho BLE Mesh Node...");
    esp_err_t err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Không thể bật chế độ provisioning (err %d)", err);
    } else {
        ESP_LOGI(TAG, "BLE Mesh Node đã sẵn sàng để provisioning.");
    }
    return err;
}