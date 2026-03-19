# THIẾT KẾ THUẬT TOÁN VÀ FIRMWARE

## (Software Design - Smart Light Gateway)

---

## MỤC LỤC

1. [Tổng quan hệ thống](#1-tổng-quan-hệ-thống)
2. [Kiến trúc phần mềm](#2-kiến-trúc-phần-mềm)
3. [Cấu trúc dự án và các thành phần](#3-cấu-trúc-dự-án-và-các-thành-phần)
4. [Thiết kế các module chính](#4-thiết-kế-các-module-chính)
5. [Luồng hoạt động và thuật toán](#5-luồng-hoạt-động-và-thuật-toán)
6. [Giao thức truyền thông](#6-giao-thức-truyền-thông)
7. [Quản lý bộ nhớ và lưu trữ](#7-quản-lý-bộ-nhớ-và-lưu-trữ)
8. [Xử lý lỗi và khôi phục](#8-xử-lý-lỗi-và-khôi-phục)
9. [Bảo mật](#9-bảo-mật)

---

## 1. TỔNG QUAN HỆ THỐNG

### 1.1. Mô tả chung

Smart Light Gateway là thiết bị trung tâm trong hệ thống chiếu sáng thông minh, đóng vai trò cầu nối giữa:

- **Ứng dụng di động/Cloud** (thông qua Wi-Fi/MQTT)
- **Các thiết bị đèn thông minh** (thông qua BLE Mesh)

### 1.2. Nền tảng phần cứng

| Thành phần            | Thông số                                      |
| --------------------- | --------------------------------------------- |
| **Vi điều khiển**     | ESP32-S3                                      |
| **Hệ điều hành**      | FreeRTOS                                      |
| **Framework**         | ESP-IDF (Espressif IoT Development Framework) |
| **Kết nối không dây** | Wi-Fi 2.4GHz, Bluetooth Low Energy 5.0        |

### 1.3. Chức năng chính

```
┌─────────────────────────────────────────────────────────────────┐
│                    SMART LIGHT GATEWAY                          │
├─────────────────────────────────────────────────────────────────┤
│  • Kết nối và quản lý mạng Wi-Fi                                │
│  • Giao tiếp MQTT với Cloud/Server                              │
│  • Provisioning và quản lý mạng BLE Mesh                        │
│  • Điều khiển các thiết bị đèn trong mạng Mesh                  │
│  • Lưu trữ cấu hình và trạng thái thiết bị                      │
│  • Cung cấp giao diện UDP cho cấu hình ban đầu                  │
│  • Quản lý các chế độ hoạt động (Normal, Provisioning, Config)  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. KIẾN TRÚC PHẦN MỀM

### 2.1. Kiến trúc phân lớp (Layered Architecture)

```
┌─────────────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │  app_logic  │  │ app_event   │  │    device_control       │  │
│  │             │  │  _handler   │  │                         │  │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                    SERVICE LAYER                                 │
│  ┌────────────┐ ┌─────────────┐ ┌──────────────┐ ┌───────────┐  │
│  │   wifi     │ │   mqtt      │ │     udp      │ │ ble_mesh  │  │
│  │  _manager  │ │  _service   │ │   _service   │ │   _core   │  │
│  └────────────┘ └─────────────┘ └──────────────┘ └───────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                    SYSTEM SERVICES LAYER                         │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ sys_event_queue │  │   sys_nvs    │  │  device_identity   │  │
│  └─────────────────┘  └──────────────┘  └────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                 HARDWARE ABSTRACTION LAYER (HAL)                 │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │   board_led     │  │ board_button │  │     led_strip      │  │
│  └─────────────────┘  └──────────────┘  └────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                    ESP-IDF / FreeRTOS                            │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────────────┐  │
│  │ WiFi │ │  BT  │ │ GPIO │ │ NVS  │ │ MQTT │ │   FreeRTOS   │  │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2. Mô hình Event-Driven Architecture

Firmware được thiết kế theo mô hình **Event-Driven** (hướng sự kiện), cho phép:

- Xử lý bất đồng bộ các sự kiện từ nhiều nguồn
- Giảm coupling giữa các module
- Dễ dàng mở rộng và bảo trì

```
                    ┌────────────────────────┐
                    │   SYS_EVENT_QUEUE      │
                    │   (FreeRTOS Queue)     │
                    └───────────┬────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐      ┌───────────────┐      ┌───────────────┐
│  WiFi Events  │      │  BLE Mesh     │      │   Button      │
│  CONNECTED    │      │  Events       │      │   Events      │
│  DISCONNECTED │      │  PROV_COMPLETE│      │   PRESSED     │
└───────────────┘      │  ONOFF_SET    │      │   HOLD_3S     │
                       └───────────────┘      │   HOLD_5S     │
                                              └───────────────┘
```

---

## 3. CẤU TRÚC DỰ ÁN VÀ CÁC THÀNH PHẦN

### 3.1. Cấu trúc thư mục

```
smart_light_gateway_fw/
├── main/
│   ├── main.c                    # Entry point của ứng dụng
│   └── CMakeLists.txt
├── components/
│   ├── app_logic/                # Logic nghiệp vụ chính
│   │   ├── app_task.c            # Task chính xử lý sự kiện
│   │   ├── app_event_handler.c   # Handlers cho các sự kiện
│   │   └── include/
│   ├── ble_mesh_core/            # Lõi BLE Mesh
│   │   ├── ble_mesh_init.c       # Khởi tạo BLE Mesh stack
│   │   ├── ble_mesh_callbacks.c  # Xử lý callbacks từ Mesh
│   │   ├── ble_mesh_composition.c# Định nghĩa Mesh models
│   │   ├── bluetooth_init.c      # Khởi tạo Bluetooth
│   │   └── include/
│   ├── wifi_manager/             # Quản lý kết nối Wi-Fi
│   │   ├── wifi_manager.c
│   │   └── wifi_manager.h
│   ├── network_manager/          # Các dịch vụ mạng
│   │   ├── mqtt_service.c        # Client MQTT
│   │   ├── udp_service.c         # Server UDP cho config
│   │   └── device_control.c      # Xử lý lệnh điều khiển
│   ├── sys_services/             # Dịch vụ hệ thống
│   │   ├── sys_event_queue.c     # Hàng đợi sự kiện trung tâm
│   │   └── sys_nvs.c             # Lưu trữ Non-Volatile
│   ├── board_hal/                # Hardware Abstraction Layer
│   │   ├── board_led.c           # Điều khiển LED trạng thái
│   │   └── board_button.c        # Xử lý nút bấm
│   ├── device_identity/          # Định danh thiết bị
│   ├── task_config/              # Cấu hình các FreeRTOS tasks
│   └── led_strip/                # Driver LED strip (nếu có)
├── certs/                        # Chứng chỉ TLS/SSL
│   └── hivemq_cloud_ca.crt
├── partitions.csv                # Bảng phân vùng flash
├── sdkconfig                     # Cấu hình SDK
└── CMakeLists.txt
```

### 3.2. Bảng mô tả các Component

| Component           | Chức năng                                             | Dependencies                              |
| ------------------- | ----------------------------------------------------- | ----------------------------------------- |
| **app_logic**       | Xử lý logic nghiệp vụ chính, quản lý state machine    | sys_services, wifi_manager, ble_mesh_core |
| **ble_mesh_core**   | Triển khai BLE Mesh protocol, provisioning, messaging | ESP BLE Mesh APIs                         |
| **wifi_manager**    | Quản lý kết nối Wi-Fi (STA/AP mode), auto-reconnect   | ESP WiFi APIs                             |
| **network_manager** | MQTT client, UDP server, xử lý lệnh điều khiển        | wifi_manager                              |
| **sys_services**    | Event queue, NVS storage, các tiện ích hệ thống       | FreeRTOS                                  |
| **board_hal**       | Trừu tượng hóa phần cứng (LED, Button)                | ESP GPIO Driver                           |
| **device_identity** | Quản lý định danh duy nhất của thiết bị               | ESP MAC APIs                              |
| **task_config**     | Định nghĩa cấu hình cho các FreeRTOS tasks            | FreeRTOS                                  |

---

## 4. THIẾT KẾ CÁC MODULE CHÍNH

### 4.1. Module System Event Queue (sys_event_queue)

#### 4.1.1. Mô tả

Đây là trung tâm điều phối sự kiện của toàn bộ hệ thống, sử dụng FreeRTOS Queue để truyền thông điệp giữa các module một cách thread-safe.

#### 4.1.2. Định nghĩa các loại sự kiện

```c
typedef enum {
    EVENT_SYSTEM_STARTUP,              // Hệ thống khởi động
    EVENT_NETWORK_CONNECTED,           // Kết nối Internet thành công
    EVENT_NETWORK_DISCONNECTED,        // Mất kết nối Internet
    EVENT_MESH_PROV_COMPLETE,          // Provisioning hoàn tất
    EVENT_MESH_CONFIG_RECEIVED,        // Nhận được cấu hình Mesh
    EVENT_MESH_GENERIC_ONOFF_SET_RECEIVED,    // Nhận lệnh OnOff
    EVENT_MESH_GENERIC_ONOFF_SET_ACK_RECEIVED,// Nhận ACK OnOff Set
    EVENT_MESH_GENERIC_ONOFF_GET_ACK_RECEIVED,// Nhận ACK OnOff Get
    EVENT_BUTTON_PRESSED,              // Nút bấm được nhấn
    EVENT_BUTTON_RELEASED,             // Nút bấm được thả
    EVENT_BUTTON_HOLD_3S,              // Giữ nút 3 giây
    EVENT_BUTTON_HOLD_5S,              // Giữ nút 5 giây
} app_event_type_t;
```

#### 4.1.3. Cấu trúc dữ liệu sự kiện

```c
typedef struct {
    esp_ble_mesh_msg_ctx_t ctx;  // Context của tin nhắn BLE Mesh
    uint8_t onoff_state;          // Trạng thái On/Off
} event_data_onoff_set_t;

typedef struct {
    app_event_type_t type;        // Loại sự kiện
    union {
        event_data_onoff_set_t onoff_set;  // Dữ liệu cho sự kiện OnOff
        // Có thể mở rộng thêm các loại dữ liệu khác
    } data;
} app_event_t;
```

#### 4.1.4. API công khai

```c
esp_err_t sys_event_queue_init(void);                           // Khởi tạo
esp_err_t sys_event_queue_send(const app_event_t *event);       // Gửi từ Task
esp_err_t sys_event_queue_send_from_isr(const app_event_t *event); // Gửi từ ISR
esp_err_t sys_event_queue_receive(app_event_t *event, TickType_t timeout);
void sys_event_queue_deinit(void);                              // Hủy
```

#### 4.1.5. Sơ đồ luồng xử lý sự kiện

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  WiFi Event  │────▶│              │     │              │
└──────────────┘     │              │     │              │
                     │   EVENT      │     │   APP_TASK   │
┌──────────────┐     │   QUEUE      │────▶│   (Loop)     │
│  BLE Mesh    │────▶│              │     │              │
│  Callback    │     │  (10 items)  │     │  switch()    │
└──────────────┘     │              │     │              │
                     │              │     │              │
┌──────────────┐     │              │     │              │
│  Button ISR  │────▶│              │     │              │
└──────────────┘     └──────────────┘     └──────────────┘
```

---

### 4.2. Module App Logic (app_logic)

#### 4.2.1. Mô tả

Module này chứa task chính của ứng dụng, xử lý tất cả các sự kiện và điều phối các module khác.

#### 4.2.2. Task chính - Event Loop

```c
static void app_main_task(void *arg) {
    // Gửi sự kiện khởi động
    app_event_t startup_event = { .type = EVENT_SYSTEM_STARTUP };
    sys_event_queue_send(&startup_event);

    app_event_t event;
    for (;;) {
        if (sys_event_queue_receive(&event, portMAX_DELAY) == ESP_OK) {
            switch (event.type) {
                case EVENT_SYSTEM_STARTUP:
                    // Xử lý khởi động
                    break;
                case EVENT_NETWORK_CONNECTED:
                    handle_network_connected();
                    break;
                case EVENT_MESH_PROV_COMPLETE:
                    // Bắt đầu kết nối Wi-Fi sau provisioning
                    break;
                case EVENT_BUTTON_HOLD_3S:
                    wifi_manager_switch_to_ap();  // Chuyển sang AP mode
                    break;
                // ... các case khác
            }
        }
    }
}
```

#### 4.2.3. State Machine của Boot Flag

```
┌─────────────────────────────────────────────────────────────────────┐
│                     BOOT FLAG STATE MACHINE                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────────────┐                                               │
│  │ BOOT_FLAG_NORMAL │◀───────────────────────────────┐              │
│  │     _START       │                                │              │
│  └────────┬─────────┘                                │              │
│           │ (Nút giữ 5s)                             │              │
│           ▼                                          │              │
│  ┌──────────────────┐      Provisioning      ┌──────┴───────┐      │
│  │ BOOT_FLAG_ADDING │     Complete           │ BOOT_FLAG    │      │
│  │ _GATEWAY_MANUAL  │ ─────────────────────▶ │ _ADDED_GW    │      │
│  └────────┬─────────┘                        └──────────────┘      │
│           │ (Nút bấm ngắn)                                         │
│           └────────────────────────────────────────▶ Reset         │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

### 4.3. Module BLE Mesh Core (ble_mesh_core)

#### 4.3.1. Mô tả

Module này triển khai toàn bộ chức năng BLE Mesh theo chuẩn Bluetooth SIG, bao gồm provisioning, configuration và messaging.

#### 4.3.2. Kiến trúc BLE Mesh

```
┌─────────────────────────────────────────────────────────────────┐
│                    BLE MESH NODE STRUCTURE                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                      ELEMENT 0                              │ │
│  │  ┌─────────────────┐  ┌─────────────────┐                  │ │
│  │  │  SIG Models     │  │  Vendor Models  │                  │ │
│  │  │                 │  │  (Custom)       │                  │ │
│  │  │ • Config Server │  │                 │                  │ │
│  │  │ • Health Server │  │ • VND_LED_CTRL  │                  │ │
│  │  │ • OnOff Server  │  │                 │                  │ │
│  │  │ • OnOff Client  │  │                 │                  │ │
│  │  └─────────────────┘  └─────────────────┘                  │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.3.3. Các Model được triển khai

| Model                | Model ID         | Vai trò                 |
| -------------------- | ---------------- | ----------------------- |
| Configuration Server | 0x0000           | Quản lý cấu hình node   |
| Health Server        | 0x0002           | Báo cáo tình trạng node |
| Generic OnOff Server | 0x1000           | Nhận lệnh bật/tắt       |
| Generic OnOff Client | 0x1001           | Gửi lệnh bật/tắt        |
| Vendor Model         | CID_ESP (0x02E5) | Model tùy chỉnh         |

#### 4.3.4. Vendor Model Opcodes

```c
#define CID_ESP 0x02E5  // Company ID của Espressif

#define MESH_VND_MODEL_OP_GET_STATE       ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define MESH_VND_MODEL_OP_SET_STATE       ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)
#define MESH_VND_MODEL_OP_SET_STATE_UNACK ESP_BLE_MESH_MODEL_OP_3(0x03, CID_ESP)
#define MESH_VND_MODEL_OP_STATUS          ESP_BLE_MESH_MODEL_OP_3(0x04, CID_ESP)
```

#### 4.3.5. API điều khiển BLE Mesh

```c
// Khởi tạo BLE Mesh stack
esp_err_t ble_mesh_core_init(void);

// Gửi lệnh OnOff (có xác nhận)
esp_err_t ble_mesh_core_send_onoff_set(uint16_t dest_addr, uint8_t onoff);

// Gửi lệnh OnOff (không cần xác nhận)
esp_err_t ble_mesh_core_send_onoff_set_unack(uint16_t dest_addr, uint8_t onoff);

// Truy vấn trạng thái OnOff
esp_err_t ble_mesh_core_send_onoff_get(uint16_t dest_addr);

// Publish trạng thái
esp_err_t ble_mesh_core_publish_onoff_status(void);

// Reset local node (xóa provisioning)
esp_err_t ble_mesh_core_node_local_reset(void);

// Bật chế độ provisioning
esp_err_t ble_mesh_core_node_prov_enable(void);
```

#### 4.3.6. Luồng Provisioning

```
┌─────────────┐                                    ┌─────────────┐
│   Gateway   │                                    │ Provisioner │
│   (Node)    │                                    │   (Phone)   │
└──────┬──────┘                                    └──────┬──────┘
       │                                                  │
       │◀─────────── Scan for unprov. devices ───────────│
       │                                                  │
       │  Unprovisioned Device Beacon (UUID, OOB info)   │
       │────────────────────────────────────────────────▶│
       │                                                  │
       │◀───────────── Provisioning Invite ──────────────│
       │                                                  │
       │─────────────── Capabilities ───────────────────▶│
       │                                                  │
       │◀──────────────── Start ─────────────────────────│
       │                                                  │
       │◀───────────── Public Key ───────────────────────│
       │                                                  │
       │─────────────── Public Key ─────────────────────▶│
       │                                                  │
       │◀──────────── Confirmation ──────────────────────│
       │                                                  │
       │─────────────── Confirmation ───────────────────▶│
       │                                                  │
       │◀──────────────── Random ────────────────────────│
       │                                                  │
       │─────────────────── Random ─────────────────────▶│
       │                                                  │
       │◀─────────── Provisioning Data ──────────────────│
       │   (NetKey, KeyIndex, Flags, IV Index, Address)  │
       │                                                  │
       │─────────────── Complete ───────────────────────▶│
       │                                                  │
       │      EVENT_MESH_PROV_COMPLETE triggered         │
       │                                                  │
```

---

### 4.4. Module WiFi Manager (wifi_manager)

#### 4.4.1. Mô tả

Quản lý toàn bộ kết nối Wi-Fi, hỗ trợ cả hai chế độ Station (STA) và Access Point (AP).

#### 4.4.2. Các chế độ hoạt động

```
┌─────────────────────────────────────────────────────────────────┐
│                    WI-FI OPERATION MODES                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────┐           ┌─────────────────┐              │
│  │   STATION MODE  │           │  ACCESS POINT   │              │
│  │                 │           │      MODE       │              │
│  │ • Kết nối Router│           │                 │              │
│  │ • Internet access│          │ • SSID: GATEWAY │              │
│  │ • MQTT client   │◀────────▶│   _XXXX         │              │
│  │                 │  Switch   │ • UDP Server    │              │
│  │                 │  (3s hold)│ • Config mode   │              │
│  └─────────────────┘           └─────────────────┘              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.4.3. Thuật toán Auto-Reconnect

```c
#define WIFI_MAXIMUM_RETRY  5

static void event_handler(...) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP (%d/%d)",
                     s_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect after %d retries.",
                     WIFI_MAXIMUM_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        // Lưu credentials vào NVS
        nvs_storage_write_wifi_creds(s_ssid, s_pass);
        // Gửi event kết nối thành công
        app_event_t app_evt = { .type = EVENT_NETWORK_CONNECTED };
        sys_event_queue_send(&app_evt);
    }
}
```

#### 4.4.4. API công khai

```c
esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect_start(const char *ssid, const char *pass);
esp_err_t wifi_manager_switch_to_ap(void);
esp_err_t wifi_manager_switch_to_sta(const char *ssid, const char *pass);
void wifi_manager_disconnect(void);
esp_err_t wifi_manager_wait_for_ip(void);
```

---

### 4.5. Module Network Manager

#### 4.5.1. MQTT Service

##### Mô tả

Client MQTT với hỗ trợ TLS/SSL để giao tiếp với Cloud broker (HiveMQ Cloud).

##### Cấu hình MQTT

```c
typedef struct {
    char broker_url[128];   // URL broker (mqtts://...)
    char username[64];       // Tên đăng nhập
    char password[64];       // Mật khẩu
    char topic_sub[64];      // Topic subscribe
    char topic_pub[64];      // Topic publish
    char topic_alert[64];    // Topic cảnh báo
} mqtt_config_t;
```

##### Tính năng Last Will and Testament

```c
const esp_mqtt_client_config_t mqtt_cfg = {
    .session.last_will.topic = "/devices/esp32_device_01/status",
    .session.last_will.msg = "offline",
    .session.last_will.msg_len = strlen("offline"),
    .session.last_will.qos = 1,
    .session.last_will.retain = true,
    // ... các cấu hình khác
};
```

##### Quản lý Subscription (Linked List)

```c
typedef struct subscription_node {
    char *topic;
    mqtt_data_callback_t callback;
    int qos;
    struct subscription_node *next;
} subscription_t;

// Thread-safe với mutex
static SemaphoreHandle_t g_subscriptions_mutex = NULL;
```

#### 4.5.2. UDP Service

##### Mô tả

Server UDP cho phép cấu hình thiết bị qua mạng local khi ở chế độ AP.

##### Cấu trúc hoạt động

```
┌─────────────────────────────────────────────────────────────────┐
│                      UDP SERVICE FLOW                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────┐      UDP Port 3333       ┌──────────┐             │
│  │  Mobile  │ ──────────────────────▶ │  Gateway │             │
│  │   App    │                          │  (AP)    │             │
│  │          │ ◀────────────────────── │          │             │
│  └──────────┘      JSON Response       └──────────┘             │
│                                                                  │
│  Commands:                                                       │
│  • CmdSetDeviceConfig: Cấu hình Wi-Fi & MQTT                    │
│  • CmdGetDeviceId: Lấy định danh thiết bị                       │
│  • CmdGetData: Truy vấn trạng thái                              │
│  • CmdSetData: Điều khiển thiết bị                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.5.3. Device Control

##### Xử lý Command JSON

```c
// Format lệnh điều khiển
{
    "name": "CmdSetData",
    "value": {
        "address": 0x0001,
        "state": 1
    }
}

// Format lệnh cấu hình
{
    "name": "CmdSetDeviceConfig",
    "value": {
        "wifi_ssid": "MyNetwork",
        "wifi_pass": "password123",
        "mqtt_url": "mqtts://broker.hivemq.com:8883",
        "mqtt_user": "username",
        "mqtt_pass": "password",
        "mqtt_sub": "smartLight/gateway/",
        "mqtt_pub": "smartLight/app/"
    }
}
```

##### Danh sách Command

| Command              | Mô tả                            |
| -------------------- | -------------------------------- |
| `CmdSetDeviceConfig` | Cấu hình Wi-Fi và MQTT           |
| `CmdGetDeviceId`     | Lấy MAC address và loại thiết bị |
| `CmdGetData`         | Truy vấn trạng thái thiết bị     |
| `CmdSetData`         | Điều khiển bật/tắt đèn           |
| `CmdGetStatus`       | Lấy trạng thái hệ thống          |
| `CmdReset`           | Reset thiết bị                   |

---

### 4.6. Module Board HAL

#### 4.6.1. Board Button

##### Thuật toán phát hiện sự kiện nút bấm

```c
#define BUTTON_SCAN_PERIOD_MS   10   // Chu kỳ quét 10ms
#define BUTTON_WAIT_SHORT       5    // 50ms - phát hiện bấm
#define BUTTON_WAIT_3S          300  // 3000ms - giữ 3 giây
#define BUTTON_WAIT_5S          500  // 5000ms - giữ 5 giây

static void button_process(void) {
    button_state_prev = button_state_cur;
    button_state_cur = button_analys();  // Đọc GPIO

    if (button_state_prev == button_state_cur) {
        button_repeat++;  // Tăng counter nếu trạng thái không đổi
    } else {
        button_repeat = 0;  // Reset counter khi thay đổi
    }

    button_state_change(button_state_cur, button_repeat);
}
```

##### State Machine nút bấm

```
┌─────────────────────────────────────────────────────────────────┐
│                    BUTTON STATE MACHINE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│    IDLE ──────▶ PRESSED ──────▶ HOLD_3S ──────▶ HOLD_5S        │
│      ▲             │               │               │            │
│      │             ▼               ▼               ▼            │
│      └──────── RELEASED ◀──────────┴───────────────┘            │
│                                                                  │
│  Events generated:                                               │
│  • 50ms pressed  → EVENT_BUTTON_PRESSED                         │
│  • 3s held       → EVENT_BUTTON_HOLD_3S (Switch to AP)          │
│  • 5s held       → EVENT_BUTTON_HOLD_5S (Factory reset)         │
│  • Released      → (internal state update)                       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

#### 4.6.2. Board LED

##### Các chế độ LED theo Boot Flag

| Boot Flag                         | Chế độ LED              | Ý nghĩa               |
| --------------------------------- | ----------------------- | --------------------- |
| `BOOT_FLAG_NORMAL_START`          | Nhấp nháy chậm (500ms)  | Đang chờ provisioning |
| `BOOT_FLAG_ADDING_GATEWAY_MANUAL` | Nhấp nháy nhanh (100ms) | Đang ở chế độ config  |
| `BOOT_FLAG_ADDED_GATEWAY`         | Sáng liên tục           | Đã được provision     |

---

## 5. LUỒNG HOẠT ĐỘNG VÀ THUẬT TOÁN

### 5.1. Luồng khởi động hệ thống

```
┌─────────────────────────────────────────────────────────────────┐
│                    SYSTEM STARTUP FLOW                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐                                           │
│  │    app_main()    │                                           │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │ nvs_storage_init │──▶ Đọc boot_flag từ NVS                   │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │wifi_manager_init │──▶ Khởi tạo Wi-Fi stack                   │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  cmd_test_start  │──▶ Khởi tạo console test                  │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │udp_service_init  │──▶ Khởi tạo UDP server                    │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │ led/button_init  │──▶ Khởi tạo HAL                           │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │  app_logic_init  │──▶ Tạo event queue & main task            │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                 boot_flag check                           │   │
│  │                                                           │   │
│  │   ┌─────────────────────────────────────────────────┐    │   │
│  │   │ BOOT_FLAG_ADDING_GATEWAY_MANUAL?                │    │   │
│  │   │                                                  │    │   │
│  │   │   YES ──▶ Return (chờ config qua UDP)           │    │   │
│  │   │                                                  │    │   │
│  │   │   NO  ──▶ Tiếp tục khởi tạo Bluetooth           │    │   │
│  │   └─────────────────────────────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────────┘   │
│           │                                                      │
│           ▼ (Nếu không phải Adding Gateway Manual)              │
│  ┌──────────────────┐                                           │
│  │ bluetooth_init() │──▶ Khởi tạo BT controller                 │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │ble_mesh_core_init│──▶ Khởi tạo BLE Mesh stack               │
│  └────────┬─────────┘                                           │
│           │                                                      │
│           ▼                                                      │
│  ┌──────────────────┐                                           │
│  │   Main task      │──▶ Event loop đang chạy                   │
│  │   running...     │                                           │
│  └──────────────────┘                                           │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2. Luồng điều khiển thiết bị (từ Cloud đến đèn)

```
┌─────────────────────────────────────────────────────────────────┐
│               DEVICE CONTROL FLOW (Cloud → Light)                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────┐     MQTT      ┌──────────┐    BLE    ┌─────────┐  │
│  │  Cloud  │ ────────────▶ │ Gateway  │ ────────▶ │  Light  │  │
│  │ Server  │               │          │   Mesh    │  Node   │  │
│  └─────────┘               └──────────┘           └─────────┘  │
│       │                         │                       │       │
│       │ 1. Publish command      │                       │       │
│       │    to topic             │                       │       │
│       │ ────────────────────────▶                       │       │
│       │                         │                       │       │
│       │                         │ 2. mqtt_event_handler │       │
│       │                         │    receives data      │       │
│       │                         │                       │       │
│       │                         │ 3. device_control_    │       │
│       │                         │    cmd_handle()       │       │
│       │                         │                       │       │
│       │                         │ 4. Parse JSON:        │       │
│       │                         │    {"name":"CmdSetData", │    │
│       │                         │     "value":{         │       │
│       │                         │       "address":0x01, │       │
│       │                         │       "state":1       │       │
│       │                         │     }}                │       │
│       │                         │                       │       │
│       │                         │ 5. ble_mesh_core_    │       │
│       │                         │    send_onoff_set()  │       │
│       │                         │ ──────────────────────▶       │
│       │                         │                       │       │
│       │                         │      6. OnOff Set     │       │
│       │                         │         (Generic)     │       │
│       │                         │                       │       │
│       │                         │ ◀──────────────────────       │
│       │                         │      7. Status ACK    │       │
│       │                         │                       │       │
│       │ ◀────────────────────────                       │       │
│       │  8. Publish response    │                       │       │
│       │                         │                       │       │
└─────────────────────────────────────────────────────────────────┘
```

### 5.3. Thuật toán xử lý sự kiện chính

```
┌─────────────────────────────────────────────────────────────────┐
│                    MAIN EVENT PROCESSING                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  START                                                          │
│    │                                                            │
│    ▼                                                            │
│  ┌────────────────────────────────────┐                         │
│  │ Wait for event from queue          │◀───────────────┐       │
│  │ (blocking with portMAX_DELAY)      │                │       │
│  └─────────────────┬──────────────────┘                │       │
│                    │                                    │       │
│                    ▼                                    │       │
│  ┌────────────────────────────────────┐                │       │
│  │ Switch on event.type               │                │       │
│  └─────────────────┬──────────────────┘                │       │
│                    │                                    │       │
│    ┌───────────────┼───────────────┬──────────────┐    │       │
│    │               │               │              │    │       │
│    ▼               ▼               ▼              ▼    │       │
│ ┌──────┐      ┌──────┐       ┌──────┐       ┌──────┐  │       │
│ │SYSTEM│      │NETWORK│      │ MESH │       │BUTTON│  │       │
│ │START │      │EVENTS │      │EVENTS│       │EVENTS│  │       │
│ └──┬───┘      └──┬───┘       └──┬───┘       └──┬───┘  │       │
│    │             │              │              │       │       │
│    ▼             ▼              ▼              ▼       │       │
│ ┌──────────────────────────────────────────────────┐  │       │
│ │ Execute corresponding handler function           │  │       │
│ │                                                  │  │       │
│ │ • handle_network_connected()                     │  │       │
│ │ • handle_network_disconnected()                  │  │       │
│ │ • handle_onoff_set_event()                       │  │       │
│ │ • wifi_manager_switch_to_ap()                    │  │       │
│ │ • esp_restart()                                  │  │       │
│ └──────────────────────────────────────────────────┘  │       │
│                    │                                   │       │
│                    └───────────────────────────────────┘       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. GIAO THỨC TRUYỀN THÔNG

### 6.1. BLE Mesh Message Format

#### Generic OnOff Set Message

```
┌────────────────────────────────────────────────────────────────┐
│                   GENERIC ONOFF SET MESSAGE                     │
├────────┬───────────┬──────────────────────────────────────────┤
│ Field  │   Size    │              Description                  │
├────────┼───────────┼──────────────────────────────────────────┤
│ Opcode │ 2 bytes   │ 0x8202 (SET) or 0x8203 (SET_UNACK)       │
│ OnOff  │ 1 byte    │ 0x00 = OFF, 0x01 = ON                    │
│ TID    │ 1 byte    │ Transaction Identifier (0-255)           │
│ Trans  │ 1 byte    │ Transition Time (optional)               │
│ Delay  │ 1 byte    │ Message execution delay (optional)       │
└────────┴───────────┴──────────────────────────────────────────┘
```

### 6.2. MQTT Message Format

#### Command Message (Cloud → Gateway)

```json
{
  "name": "CmdSetData",
  "value": {
    "address": 1,
    "state": 1
  },
  "timestamp": 1699876543
}
```

#### Response Message (Gateway → Cloud)

```json
{
  "name": "CmdGetData",
  "devT": 3017,
  "address": 1,
  "state": 1
}
```

### 6.3. UDP Configuration Protocol

#### Request Format

```json
{
  "name": "CmdSetDeviceConfig",
  "value": {
    "wifi_ssid": "NetworkName",
    "wifi_pass": "NetworkPassword",
    "mqtt_url": "mqtts://broker.hivemq.com:8883",
    "mqtt_user": "username",
    "mqtt_pass": "password",
    "mqtt_sub": "smartLight/gateway/001/",
    "mqtt_pub": "smartLight/app/"
  }
}
```

---

## 7. QUẢN LÝ BỘ NHỚ VÀ LƯU TRỮ

### 7.1. Partition Table

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLASH PARTITION LAYOUT                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Offset      Size      Name        Type      SubType            │
│  ─────────────────────────────────────────────────────          │
│  0x009000    0x006000  nvs         data      nvs                │
│  0x00F000    0x001000  phy_init    data      phy                │
│  0x010000    0x500000  factory     app       factory            │
│  0x510000    0x500000  ota_1       app       ota_0              │
│  0xA10000    0x3E0000  storage     data      spiffs             │
│  0xDF0000    0x002000  flags       data      ota                │
│                                                                  │
│  Total: 16MB Flash                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2. NVS Storage Structure

```
┌─────────────────────────────────────────────────────────────────┐
│                    NVS NAMESPACES & KEYS                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Namespace: "wifi_config"                                        │
│  ├── ssid          : string (32 bytes)                          │
│  └── password      : string (64 bytes)                          │
│                                                                  │
│  Namespace: "mqtt_config"                                        │
│  ├── broker_url    : string (128 bytes)                         │
│  ├── username      : string (64 bytes)                          │
│  ├── password      : string (64 bytes)                          │
│  ├── topic_sub     : string (64 bytes)                          │
│  ├── topic_pub     : string (64 bytes)                          │
│  └── topic_alert   : string (64 bytes)                          │
│                                                                  │
│  Namespace: "led_states"                                         │
│  ├── led1_state    : uint8                                      │
│  ├── led2_state    : uint8                                      │
│  └── led3_state    : uint8                                      │
│                                                                  │
│  Namespace: "dev_config_ns"                                      │
│  └── device_config : blob                                       │
│                                                                  │
│  Partition: "flags"                                              │
│  └── boot_flag     : uint8 (raw flash write)                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 7.3. Boot Flags

```c
typedef enum {
    BOOT_FLAG_NORMAL_START = 0x00,          // Khởi động bình thường
    BOOT_FLAG_ADDING_GATEWAY_MANUAL = 0x01, // Chế độ thêm gateway thủ công
    BOOT_FLAG_ADDED_GATEWAY = 0x02,         // Gateway đã được thêm
} boot_flag_t;
```

---

## 8. XỬ LÝ LỖI VÀ KHÔI PHỤC

### 8.1. Chiến lược xử lý lỗi

| Loại lỗi               | Hành động          | Mô tả                            |
| ---------------------- | ------------------ | -------------------------------- |
| Wi-Fi disconnect       | Auto-retry (5 lần) | Tự động kết nối lại tối đa 5 lần |
| MQTT disconnect        | Auto-reconnect     | Client tự động kết nối lại       |
| BLE Mesh timeout       | Log & continue     | Ghi log và tiếp tục hoạt động    |
| NVS read fail          | Use defaults       | Sử dụng giá trị mặc định         |
| Memory allocation fail | Log & return error | Báo lỗi và trả về mã lỗi         |

### 8.2. Watchdog và Recovery

```c
// Factory Reset khi giữ nút 5 giây
case EVENT_BUTTON_HOLD_5S:
    ESP_LOGI(TAG, "Sự kiện: Button giữ 5 giây - Factory Reset");
    led_service_set_boot_flag(BOOT_FLAG_NORMAL_START);
    // Reset BLE Mesh local data
    ble_mesh_core_node_local_reset();
    // Restart device
    esp_restart();
    break;
```

### 8.3. MQTT Last Will and Testament

```c
// Khi mất kết nối đột ngột, broker sẽ tự động publish:
.session.last_will.topic = "/devices/esp32_device_01/status",
.session.last_will.msg = "offline",
.session.last_will.qos = 1,
.session.last_will.retain = true,
```

---

## 9. BẢO MẬT

### 9.1. Bảo mật kết nối MQTT

- **TLS/SSL**: Sử dụng MQTTS (MQTT over TLS) với cổng 8883
- **Certificate verification**: Nhúng CA certificate của HiveMQ Cloud
- **Authentication**: Username/Password authentication

```c
const esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtts://...:8883",
    .broker.verification.certificate = (const char *)server_cert_pem_start,
    .credentials = {
        .username = mqtt_config.username,
        .authentication.password = mqtt_config.password,
    }
};
```

### 9.2. Bảo mật BLE Mesh

- **Provisioning security**: Sử dụng ECDH key exchange
- **Network encryption**: AES-CCM với NetKey
- **Application encryption**: AES-CCM với AppKey
- **Device authentication**: Device UUID và OOB data

### 9.3. Lưu trữ an toàn

- Credentials được lưu trong NVS (Non-Volatile Storage)
- Sử dụng ESP-IDF NVS API với checksum protection
- Boot flags lưu trong partition riêng biệt

---

## PHỤ LỤC

### A. Danh sách FreeRTOS Tasks

| Task Name   | Stack Size | Priority | Core | Chức năng        |
| ----------- | ---------- | -------- | ---- | ---------------- |
| app_task    | 4096       | 20       | 1    | Event loop chính |
| udp_server  | 8192       | 5        | Any  | UDP server       |
| button_task | 2048       | 10       | Any  | Scan button      |
| led_task    | 2048       | 5        | Any  | LED control      |

### B. Danh sách GPIO Pins

| GPIO   | Chức năng   | Mode           |
| ------ | ----------- | -------------- |
| GPIO 0 | Boot Button | Input, Pull-up |
| GPIO 2 | Status LED  | Output         |

### C. Tài liệu tham khảo

1. ESP-IDF Programming Guide
2. Bluetooth Mesh Specification v1.0.1
3. ESP-BLE-MESH User Guide
4. HiveMQ MQTT Essentials
5. FreeRTOS Reference Manual

---
