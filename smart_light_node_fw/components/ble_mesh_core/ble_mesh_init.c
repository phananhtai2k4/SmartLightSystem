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
#include "board_hal.h"
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

esp_ble_mesh_model_t *ble_mesh_core_find_vendor_model(const esp_ble_mesh_elem_t *element, uint16_t company_id, uint16_t model_id)
{
    if (element == NULL) {
        return NULL;
    }

    // Duyệt qua tất cả các Vendor Model trong element
    for (int i = 0; i < element->vnd_model_count; i++) {
        esp_ble_mesh_model_t *vnd_model = &element->vnd_models[i];
        if (vnd_model->vnd.company_id == company_id && vnd_model->vnd.model_id == model_id) {
            return vnd_model; // Tìm thấy!
        }
    }

    return NULL; // Không tìm thấy
}

esp_err_t self_bind_vendor_model(uint16_t element_addr, uint16_t app_idx, 
                                        uint16_t model_id, uint16_t company_id)
{
    esp_ble_mesh_elem_t *element = esp_ble_mesh_find_element(element_addr);
    if (!element) {
        ESP_LOGE(TAG, "self_bind: Không tìm thấy element tại địa chỉ 0x%04x", element_addr);
        return ESP_FAIL;
    }
    esp_ble_mesh_model_t *model = ble_mesh_core_find_vendor_model(element, company_id, model_id);
    if (!model) {
        ESP_LOGE(TAG, "self_bind: Không tìm thấy model (cid 0x%04x, mod_id 0x%04x)", company_id, model_id);
        return ESP_FAIL;
    }

    // Duyệt qua mảng keys của model
    for (int i = 0; i < ARRAY_SIZE(model->keys); i++) {
        if (model->keys[i] == app_idx) {
            ESP_LOGW(TAG, "self_bind: AppKey 0x%04x đã được bind vào model này rồi.", app_idx);
            return ESP_OK; // Đã được bind, không cần làm gì thêm
        }
    }

    // Tìm một vị trí trống để gán app_idx
    for (int i = 0; i < ARRAY_SIZE(model->keys); i++) {
        if (model->keys[i] == ESP_BLE_MESH_KEY_UNUSED) {
            model->keys[i] = app_idx;
            ESP_LOGI(TAG, "self_bind: Gán thành công AppKey 0x%04x vào vị trí %d của model.", app_idx, i);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "self_bind: Không còn vị trí trống để bind AppKey vào model.");
    return ESP_FAIL;
}

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
    // Bind lại Control Server
    self_bind_vendor_model(self_addr, app_idx, MESH_VND_MODEL_ID_CONTROL_SERVER, CID_ESP);

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
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);

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

esp_err_t ble_mesh_core_publish_onoff_status_by_button(void)
{
    // Lấy model handle từ biến 'onoff_server' đã được khai báo trong composition.c
    extern esp_ble_mesh_gen_onoff_srv_t onoff_server;

    // Trạng thái onoff được lưu trong srv->state.onoff
    uint8_t current_state = onoff_server.state.onoff;
    if (current_state == 0) {
        current_state = OFF_LED_BY_BUTTON; // Tắt
    } else {
        current_state = ON_LED_BY_BUTTON; // Bật
    }

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

esp_err_t ble_mesh_core_publish_onoff_status_by_app(void)
{
    // Lấy model handle từ biến 'onoff_server' đã được khai báo trong composition.c
    extern esp_ble_mesh_gen_onoff_srv_t onoff_server;

    // Trạng thái onoff được lưu trong srv->state.onoff
    uint8_t current_state = onoff_server.state.onoff;
    if (current_state == 0) {
        current_state = OFF_LED_BY_APP; // Tắt
    } else {
        current_state = ON_LED_BY_APP; // Bật
    }

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

esp_err_t ble_mesh_core_publish_onoff_status_by_leakage(void)
{
    // Lấy model handle từ biến 'onoff_server' đã được khai báo trong composition.c
    extern esp_ble_mesh_gen_onoff_srv_t onoff_server;

    // Trạng thái onoff được lưu trong srv->state.onoff
    uint8_t current_state = onoff_server.state.onoff;
    if (current_state == 0) {
        current_state = OFF_LED_BY_LEAKAGE; // Tắt
    } else {
        current_state = ON_LED_BY_LEAKAGE; // Bật
    }

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

static uint8_t encode_device_status(device_status_t *st, uint8_t *buf)
{
    uint8_t *p = buf;

    // ON/OFF
    *p++ = TLV_ONOFF;
    *p++ = 1;
    *p++ = st->onoff;

    // LIGHT
    *p++ = TLV_LIGHT_LUX;
    *p++ = 2;
    *p++ = st->light_lux & 0xFF;
    *p++ = (st->light_lux >> 8) & 0xFF;

    // CURRENT
    *p++ = TLV_CURRENT_MA;
    *p++ = 2;
    *p++ = st->current_ma & 0xFF;
    *p++ = (st->current_ma >> 8) & 0xFF;

    return (p - buf); // length
}

static uint8_t encode_device_onoff(device_status_t *st, uint8_t *buf)
{
    uint8_t *p = buf;

    *p++ = TLV_ONOFF;
    *p++ = 1;
    *p++ = st->onoff;

    return (p - buf); // length
}

esp_err_t ble_mesh_core_send_vendor_response(const esp_ble_mesh_msg_ctx_t *ctx,
                                             uint32_t opcode,
                                             const uint8_t *payload,
                                             uint16_t length)
{
    esp_err_t err;
    
    ESP_LOGI(TAG, "Đang gửi phản hồi vendor: opcode=0x%06" PRIx32 ", len=%d, đến addr=0x%04x",
             opcode, length, ctx->addr);
             
    // In ra payload sẽ gửi đi để gỡ lỗi
    ESP_LOG_BUFFER_HEX(TAG, payload, length);
    err = esp_ble_mesh_server_model_send_msg(&vnd_models_root[1],
                                           (esp_ble_mesh_msg_ctx_t *)ctx,
                                           opcode,
                                           length,
                                           (uint8_t *)payload);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Gửi phản hồi vendor thất bại (err %d)", err);
    }

    return err;
}

esp_err_t ble_mesh_core_handle_ssw_get_data(const esp_ble_mesh_msg_ctx_t *ctx)
{
    uint8_t payload[20];
    uint8_t payload_len = encode_device_status(&saved_status, payload);

    //Gửi phản hồi với opcode SSW Get Data Status
    return ble_mesh_core_send_vendor_response(ctx, MESH_VND_OP_SSW_GET_DATA_STATUS, payload, payload_len);
}

//hàm chủ động publish trạng thái hiện tại của thiết bị dựa vào hàm esp_ble_mesh_model_publish(esp_ble_mesh_model_t *model, uint32_t opcode, uint16_t length, uint8_t *data, esp_ble_mesh_dev_role_t device_role)
esp_err_t ble_mesh_core_publish_current_status(void)
{
    uint8_t payload[20];
    //random dữ liệu để test
    saved_status.current_ma = random() % 1000; // 0-999 mA
    saved_status.light_lux = random() % 1000; // 0-999 lux
    uint8_t payload_len = encode_device_status(&saved_status, payload);

    ESP_LOGI(TAG, "Publishing current status: onoff=%d, lux=%d, current=%dmA",
             saved_status.onoff, saved_status.light_lux, saved_status.current_ma);
    return esp_ble_mesh_model_publish(&vnd_models_root[0], MESH_VND_OP_SSW_GET_DATA_STATUS, payload_len, payload, ROLE_NODE);
}