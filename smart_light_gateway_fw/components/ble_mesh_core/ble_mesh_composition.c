/*
 * ble_mesh_composition.c - Định nghĩa cấu trúc (Composition Data) của Node
 *
 * File này định nghĩa tất cả các Element và Model tạo nên chức năng
 * của Smart Relay Node trong mạng BLE Mesh.
 */

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "ble_mesh_core.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#define TAG "BLE_MESH_COMPOSITION"

// --- Biến cho Configuration Server Model ---
static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 lần truyền với khoảng cách 20ms */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(control_srv_pub, 3 + 16, ROLE_NODE); // Opcode (3) + Payload tối đa (ví dụ 4 kênh * 3 byte/kênh)
static esp_ble_mesh_model_op_t control_srv_op[] = {
    ESP_BLE_MESH_MODEL_OP(MESH_VND_OP_SSW_SET_DATA, 1),
    ESP_BLE_MESH_MODEL_OP(MESH_VND_OP_SSW_GET_DATA, 0),
    ESP_BLE_MESH_MODEL_OP(MESH_VND_OP_SSW_GET_DATA_STATUS, 0),
    ESP_BLE_MESH_MODEL_OP_END,
};

// --- Các biến cho Generic OnOff Server Model ---
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_srv, 2 + 3, ROLE_NODE);
esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    },
};

// --- Các biến cho Generic OnOff Client Model ---
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_cli, 2 + 1, ROLE_NODE);
esp_ble_mesh_client_t onoff_client;

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_srv, &onoff_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_pub_cli, &onoff_client),
};

// Model Vendor cho Element 0 (root)
esp_ble_mesh_model_t vnd_models_root[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, MESH_VND_MODEL_ID_CONTROL_SERVER, control_srv_op, &control_srv_pub, NULL),
};

esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models_root),
};

esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

void ble_mesh_composition_setup_defaults(void)
{
    uint16_t self_addr = esp_ble_mesh_get_primary_element_address();
    esp_ble_mesh_model_t control_model = vnd_models_root[0];
    // if (control_model == NULL || control_model->pub == NULL) {
    //     ESP_LOGE(TAG, "Không tìm thấy Control Server Model hoặc cấu trúc pub của nó!");
    //     return;
    // }
    // control_model.pub->publish_addr = BLEMESH_TOPIC_CONTROL_SERVER_PUB;
    control_model.pub->app_idx = 0;
    control_model.pub->ttl = 3;
    control_model.pub->retransmit = ESP_BLE_MESH_PUBLISH_TRANSMIT(2, 20);
    control_model.pub->period = 0;
    root_models[1].pub->publish_addr = BLEMESH_TOPIC_CONTROL_SERVER_PUB;
    root_models[1].pub->ttl = 3;
    root_models[1].pub->retransmit = ESP_BLE_MESH_PUBLISH_TRANSMIT(2, 20);
    root_models[1].pub->app_idx = 0;
    root_models[2].pub->publish_addr = BLEMESH_TOPIC_CONTROL_SERVER_PUB;
    root_models[2].pub->ttl = 3;
    root_models[2].pub->retransmit = ESP_BLE_MESH_PUBLISH_TRANSMIT(2, 20);
    root_models[2].pub->app_idx = 0;
    // control_model.groups[0] = BLEMESH_TOPIC_CONTROL_SERVER_PUB;
    // esp_err_t esp_ble_mesh_model_subscribe_group_addr(uint16_t element_addr, uint16_t company_id,
    //                                               uint16_t model_id, uint16_t group_addr)
    esp_err_t err = esp_ble_mesh_model_subscribe_group_addr(self_addr, CID_ESP, MESH_VND_MODEL_ID_CONTROL_SERVER, BLEMESH_TOPIC_CONTROL_SERVER_PUB);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Lỗi khi subscribe group address: %d", err);
    } else {
        ESP_LOGI(TAG, "Đã subscribe group address 0x%04X cho Control Server Model", BLEMESH_TOPIC_CONTROL_SERVER_PUB);
    }
    root_models[2].groups[0] = BLEMESH_TOPIC_CONTROL_SERVER_PUB;
    ESP_LOGI(TAG, "Đã thiết lập địa chỉ publish mặc định cho Control Server là 0x%04X", control_model.pub->publish_addr);
}