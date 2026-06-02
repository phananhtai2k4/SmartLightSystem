/*
 * ble_mesh_callbacks.c - Triển khai các hàm callback cho BLE Mesh Stack
 *
 * File này chứa các hàm được đăng ký với ESP-IDF BLE Mesh stack.
 * Khi có một sự kiện từ mạng (ví dụ: nhận được tin nhắn, provisioning hoàn tất),
 * stack sẽ gọi các hàm tương ứng trong file này.
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "esp_ble_mesh_networking_api.h"

#include "ble_mesh_core.h"        // Header công khai của component này
#include "sys_event_queue.h"      // Header của hàng đợi sự kiện hệ thống
#include "board_hal.h"           // Header công khai của phần cứng đặc thù
// #include "ble_mesh_composition.h" // Để truy cập các biến model (ví dụ: onoff_server)

#define TAG "BLE_MESH_CALLBACKS"


/**
 * @brief Hàm này được gọi bởi callback để xử lý logic chung khi trạng thái thay đổi.
 * 
 * Nó sẽ gọi đến lớp logic ứng dụng và publish trạng thái mới.
 */
static void handle_onoff_state_change(esp_ble_mesh_model_t *model, esp_ble_mesh_msg_ctx_t *ctx, uint8_t new_onoff)
{
    // 1. Cập nhật trạng thái logic của model
    onoff_server.state.onoff = new_onoff;

    // 2. Đẩy sự kiện đến app_logic để xử lý nghiệp vụ (bật/tắt đèn, lưu NVS)
    app_event_t app_event = { .type = EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED };
    app_event.data.onoff_set.ctx = *ctx;
    app_event.data.onoff_set.onoff_state = new_onoff;
    sys_event_queue_send(&app_event);
}

static uint16_t g_node_address = ESP_BLE_MESH_ADDR_UNASSIGNED;

/*
 * =====================================================================================
 * CALLBACK CHO PROVISIONING
 * =====================================================================================
 */

void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                              esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "NODE_PROV_LINK_OPEN_EVT, bearer %s",
            param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "NODE_PROV_LINK_CLOSE_EVT, bearer %s",
            param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "NODE_PROV_COMPLETE_EVT");
        ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x, flags: 0x%02x, iv_index: 0x%08" PRIx32,
            param->node_prov_complete.net_idx,
            param->node_prov_complete.addr,
            param->node_prov_complete.flags,
            param->node_prov_complete.iv_index);
            g_node_address = param->node_prov_complete.addr;

        // Provisioning đã hoàn tất, gửi sự kiện này đến lớp logic ứng dụng
        app_event_t app_event = { .type = EVENT_MESH_PROV_COMPLETE };
        // Có thể điền thêm dữ liệu vào sự kiện nếu cần
        // event.data.prov_complete.addr = param->node_prov_complete.addr;
        sys_event_queue_send(&app_event);
        break;
    default:
        break;
    }
}


/*
 * =====================================================================================
 * CALLBACK CHO CONFIGURATION SERVER MODEL
 * =====================================================================================
 * Model này xử lý các cấu hình cơ bản của node sau khi provision.
 */

void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                               esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        return;
    }

    switch (param->ctx.recv_op) {
    case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
        ESP_LOGI(TAG, "CFG: APP_KEY_ADD, net_idx 0x%04x, app_idx 0x%04x",
            param->value.state_change.appkey_add.net_idx,
            param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("CONFIG_CALLBACK", param->value.state_change.appkey_add.app_key, 16);
            // Lấy thông tin cần thiết từ sự kiện
            uint16_t app_idx = param->value.state_change.appkey_add.app_idx;

            ESP_LOGW(TAG, "=========== BẮT ĐƯỢC APPKEY, BẮT ĐẦU TỰ BIND (phương pháp gán trực tiếp) ===========");

            // Bind lại OnOff Client
            esp_ble_mesh_node_bind_app_key_to_local_model(g_node_address, 0xFFFF,
                                                        ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI, app_idx);
            // Bind lại OnOff Server
            esp_ble_mesh_node_bind_app_key_to_local_model(g_node_address, 0xFFFF,
                                                        ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, app_idx);
            
            ESP_LOGW(TAG, "=========== TỰ BIND HOÀN TẤT, NODE SẴN SÀNG ===========");
        break;

    case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
        ESP_LOGI(TAG, "CFG: MODEL_APP_BIND, elem_addr 0x%04x, app_idx 0x%04x, mod_id 0x%04x",
            param->value.state_change.mod_app_bind.element_addr,
            param->value.state_change.mod_app_bind.app_idx,
            param->value.state_change.mod_app_bind.model_id);
            
        // Gửi sự kiện để app_logic có thể lưu lại app_idx nếu cần
        // app_event_t app_event = { .type = EVENT_MESH_CONFIG_RECEIVED };
        // event.data.config.app_idx = param->value.state_change.mod_app_bind.app_idx;
        // sys_event_queue_send(&app_event);
        break;

    case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
         ESP_LOGI(TAG, "CFG: MODEL_SUB_ADD, elem_addr 0x%04x, sub_addr 0x%04x, mod_id 0x%04x",
                param->value.state_change.mod_sub_add.element_addr,
                param->value.state_change.mod_sub_add.sub_addr,
                param->value.state_change.mod_sub_add.model_id);
        break;
        
    default:
        // ESP_LOGW(TAG, "Config Server, recv_op 0x%04x", param->ctx.recv_op);
        ESP_LOGW(TAG, "Config Server, recv_op(uint32_t) %lu", (unsigned long)param->ctx.recv_op);
        break;
    }
}


/*
 * =====================================================================================
 * CALLBACK CHO GENERIC SERVER MODELS (ví dụ: OnOff Server)
 * =====================================================================================
 * Xử lý các tin nhắn nhận được dành cho các chức năng Server.
 */

void ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "Generic Server, event %u, src 0x%04x, dst 0x%04x, opcode 0x%04" PRIx32,
        event, param->ctx.addr, param->ctx.recv_dst, param->ctx.recv_op);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        // Sự kiện này được kích hoạt khi trạng thái của model bị thay đổi bởi stack,
        // ví dụ khi rsp_ctrl được đặt là auto_rsp.
        ESP_LOGI(TAG, "SERVER_STATE_CHANGE: onoff_state = %d", param->value.state_change.onoff_set.onoff);
        
        // Đây là nơi quan trọng để đồng bộ trạng thái vật lý với trạng thái logic.
        // Gửi sự kiện đến app_logic để xử lý.
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {

            app_event_t app_event = { .type = EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED };
            app_event.data.onoff_set.ctx = param->ctx;
            app_event.data.onoff_set.onoff_state = param->value.state_change.onoff_set.onoff;
            sys_event_queue_send(&app_event);
        }
        break;

    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        ESP_LOGI(TAG, "SERVER_RECV_GET_MSG");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            // Logic: Ứng dụng phải tự gửi tin nhắn STATUS để trả lời.
            // Trạng thái hiện tại được lấy từ biến onoff_server.
            ESP_LOGI(TAG, "Trả lời GET request với trạng thái onoff = %d", onoff_server.state.onoff);
            esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
                ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                sizeof(onoff_server.state.onoff), &onoff_server.state.onoff);
        }
        break;

    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        ESP_LOGI(TAG, "SERVER_RECV_SET_MSG");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {

            uint8_t new_state = param->value.set.onoff.onoff;
            ESP_LOGI(TAG, "Nhận được lệnh SET, onoff = %d, tid = 0x%02x", new_state, param->value.set.onoff.tid);
            
            // TODO: Ở đây bạn có thể thêm logic kiểm tra TID để chống lặp lại lệnh nếu cần.

            // Logic: Cập nhật trạng thái và thực hiện hành động
            handle_onoff_state_change(param->model, &param->ctx, new_state);

            // Logic: Nếu là tin nhắn SET (cần xác nhận), ứng dụng phải tự gửi STATUS trả lời.
            if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
                ESP_LOGI(TAG, "Gửi lại STATUS để xác nhận lệnh SET");
                esp_ble_mesh_server_model_send_msg(param->model, &param->ctx,
                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                    sizeof(onoff_server.state.onoff), &onoff_server.state.onoff);
            }
        }
        break;
        
    default:
        ESP_LOGW(TAG, "Unknown Generic Server event: 0x%02x", event);
        break;
    }
}


/*
 * =====================================================================================
 * CALLBACK CHO GENERIC CLIENT MODELS (ví dụ: OnOff Client)
 * =====================================================================================
 * Xử lý các phản hồi (status message) từ các Server.
 */

void ble_mesh_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event,
                                esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "Generic Client, event %u, error_code %d, opcode 0x%04" PRIx32,
        event, param->error_code, param->params->opcode);

    if (param->error_code) {
        ESP_LOGE(TAG, "Client error: Sent to 0x%04x, timeout", param->params->ctx.addr);
        return;
    }

    switch (event) {
    case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            ESP_LOGI(TAG, "OnOff Get status from 0x%04x: state is %s",
                param->params->ctx.addr,
                param->status_cb.onoff_status.present_onoff ? "ON" : "OFF");
            // Có thể gửi sự kiện đến app_logic nếu cần xử lý trạng thái này
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
        if (param->params->opcode == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            ESP_LOGI(TAG, "OnOff Set status from 0x%04x: state is %s",
                param->params->ctx.addr,
                param->status_cb.onoff_status.present_onoff ? "ON" : "OFF");
            // Có thể gửi sự kiện đến app_logic để xác nhận lệnh đã được thực thi
        }
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
        // Sự kiện này xảy ra khi hết thời gian publish định kỳ của client model.
        // Ít được sử dụng.
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
        // Sự kiện này xảy ra khi gửi một tin nhắn cần xác nhận (acknowledged) nhưng không nhận được phản hồi.
        ESP_LOGW(TAG, "Client timeout: message to 0x%04x failed", param->params->ctx.addr);
        // Có thể gửi sự kiện đến app_logic để thử gửi lại.
        break;
    default:
        break;
    }
}

/*
 * =====================================================================================
 * CÁC HÀM CALLBACK VENDOR MODELS
 * =====================================================================================
 */

void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                              esp_ble_mesh_model_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT: {
        ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OPERATION_EVT");
        // Kiểm tra model ID để gọi đúng parser
        if (param->model_operation.model->vnd.model_id == MESH_VND_MODEL_ID_CONTROL_SERVER) {
            ESP_LOGI(TAG, "Control Server Model ID: 0x%04x", param->model_operation.model->vnd.model_id);
            ESP_LOGI(TAG, "Received opcode: 0x%06" PRIx32, param->model_operation.opcode);
            // parse_control_payload(param->model_operation.model,
            //                       param->model_operation.ctx,
            //                       param->model_operation.msg,
            //                       param->model_operation.length);
        } else {
            ESP_LOGW(TAG, "Unknown Vendor Model ID: 0x%04x",
                     param->model_operation.model->vnd.model_id);
        }
        break;
    }
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        ESP_LOGI(TAG, "Vendor Send Complete, opcode 0x%06" PRIx32, param->model_send_comp.opcode);
        break;
    
    case ESP_BLE_MESH_MODEL_PUBLISH_COMP_EVT:
        // ESP_LOGI(TAG, "Vendor Publish Complete");
        break; 
    default:
        ESP_LOGW(TAG, "Unknown Custom Model event: 0x%02x", event);
        break;
    }
}