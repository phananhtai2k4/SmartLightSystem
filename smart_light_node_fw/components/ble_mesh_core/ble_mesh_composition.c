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

// Định nghĩa Company ID (CID) của Espressif.
// Bạn có thể đăng ký một CID riêng cho công ty của mình.
#define CID_ESP 0x02E5

#define TAG "BLE_MESH_COMPOSITION"

/*
 * =====================================================================================
 * PHẦN 1: KHAI BÁO CÁC BIẾN ĐIỀU KHIỂN (HANDLE) VÀ CONTEXT CHO CÁC MODEL
 * =====================================================================================
 * Các biến này sẽ được liên kết với các model tương ứng.
 */

// --- Biến cho Configuration Server Model ---
// Đây là model bắt buộc, dùng để Provisioner cấu hình Node.
static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_ENABLED, // Bật tính năng Relay theo mặc định
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED, // Bật tính năng Friend nếu cần cho các node Low Power
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
    ESP_BLE_MESH_MODEL_OP_END,
};

// --- Các biến cho Generic OnOff Server Model ---
// Model này cho phép Node nhận lệnh Bật/Tắt từ các Client.
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_srv, 2 + 3, ROLE_NODE); // Context cho việc publish trạng thái
esp_ble_mesh_gen_onoff_srv_t onoff_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    },
};

// --- Các biến cho Generic OnOff Client Model ---
// Model này cho phép Node gửi lệnh Bật/Tắt đến các Server khác.
ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_cli, 2 + 1, ROLE_NODE); // Context cho việc publish của client
esp_ble_mesh_client_t onoff_client; // Handle cho client model


/*
 * =====================================================================================
 * PHẦN 2: ĐỊNH NGHĨA CÁC MẢNG MODEL VÀ ELEMENT
 * =====================================================================================
 * Đây là nơi lắp ráp các model vào các element.
 */

// Mảng chứa tất cả các model nằm trên Element chính (Root Element).
// Một Relay Node thông minh có cả chức năng Server (bị điều khiển) và Client (đi điều khiển).
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_srv, &onoff_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(&onoff_pub_cli, &onoff_client),
};

// Model Vendor cho Element 0 (root)
esp_ble_mesh_model_t vnd_models_root[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, MESH_VND_MODEL_ID_CONTROL_SERVER, control_srv_op, &control_srv_pub, NULL),
};

// Mảng chứa tất cả các Element của Node.
// Đối với một relay đơn, chúng ta chỉ cần một Element duy nhất.
// Địa chỉ của Element này sẽ là Primary Address của Node.
static esp_ble_mesh_elem_t elements[] = {
    // Định nghĩa Element tại địa chỉ offset 0, chứa các model trong root_models.
    // Không có model nào của SIG hoặc Vendor ở cấp độ thứ hai.
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models_root),
};


/*
 * =====================================================================================
 * PHẦN 3: ĐỊNH NGHĨA CẤU TRÚC TỔNG HỢP (COMPOSITION)
 * =====================================================================================
 * Đây là cấu trúc cuối cùng, mô tả toàn bộ Node.
 */

// Biến composition này sẽ được sử dụng bởi `esp_ble_mesh_init()`.
// Nó là một biến toàn cục để các file khác trong component có thể truy cập.
esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,                     // Company ID
    .element_count = ARRAY_SIZE(elements), // Số lượng element trong Node
    .elements = elements,               // Con trỏ tới mảng các element
};