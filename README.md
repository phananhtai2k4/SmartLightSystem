# Smart Light System - BLE Mesh & MQTT Gateway

---

## 🏗 Kiến trúc hệ thống

<img width="1952" height="665" alt="image" src="https://github.com/user-attachments/assets/36b6599e-bb73-4ad9-9ac1-75021a51a226" />

---

## 📁 Cấu trúc dự án

Dự án được chia thành 4 phân hệ chính:

| Thư mục | Nền tảng / Công nghệ | Mô tả chức năng chi tiết |
| :--- | :--- | :--- |
| **[`smart_light_node_fw`](./smart_light_node_fw)** | C/C++, ESP32-WROOM-32 (ESP-IDF) | **Firmware node Đèn:**<br>• Triển khai chuẩn **BLE Mesh SIG** (Generic OnOff Server/Client, Relay Node) và **Custom Vendor Model** đóng gói dữ liệu cảm biến theo định dạng **TLV** (Type-Length-Value).<br>• Thuật toán lấy mẫu ADC liên tục tính **True RMS** dòng rò (cảm biến ZMCT103C), tự động dập Relay bảo vệ an toàn tại chỗ khi phát hiện sự cố kể cả khi mất kết nối mạng.<br>• Đo lường cường độ ánh sáng môi trường qua **quang trở LDR**.<br>• Xử lý **nút bấm vật lý đa năng**: bấm đơn để đảo trạng thái đèn, nhấn giữ 3 giây để xóa cấu hình và reset Node về trạng thái chờ nạp mạng (Unprovisioned).<br>• Quản lý lưu trữ **NVS Flash**: tự động ghi nhớ và khôi phục trạng thái On/Off của đèn khi bị mất nguồn đột ngột. |
| **[`smart_light_gateway_fw`](./smart_light_gateway_fw)** | C/C++, ESP32-S3 (ESP-IDF) | **Firmware Bộ Điều Khiển Trung Tâm (Gateway):**<br>• Cầu nối chuyển đổi giao thức 2 chiều bất đối xứng giữa **Wi-Fi (MQTT TLS HiveMQ)** và **BLE Mesh**.<br>• Ứng dụng **FreeRTOS Queue** điều phối đa tác vụ bất đồng bộ, chống nghẽn và chống tràn bộ nhớ khi lưu lượng bản tin cao.<br>• Giải mã dữ liệu **TLV** từ Node; phân loại nguồn kích hoạt trạng thái đèn: do bấm nút tại chỗ, do lệnh từ App hay do tự ngắt vì dòng rò.<br>• Tích hợp dịch vụ **SNTP** đồng bộ thời gian thực qua Internet để gán nhãn thời gian (timestamp) chính xác cho từng sự kiện và cảnh báo.<br>• Quản lý lưu thông tin mạng Wi-Fi, Broker vào NVS Flash và giám sát trạng thái kết nối Online/Offline của Gateway. |
| **[`smart_light_backend`](./smart_light_backend)** | Node.js, Docker | **Dịch vụ Trung Gian Cloud:**<br>• Kết nối MQTT Broker (HiveMQ) và cơ sở dữ liệu Firebase Firestore, đồng bộ trạng thái hai chiều thời gian thực giữa thiết bị phần cứng và người dùng.<br>• **Hệ thống cảnh báo khẩn cấp (Push Notification):** Bắn thông báo khẩn qua **Firebase Cloud Messaging (FCM)** (kèm âm thanh chuông báo và hình ảnh cảnh báo) đến điện thoại người dùng ngay khi phát hiện sự cố dòng rò.<br>• Ghi nhật ký lịch sử sự kiện hoạt động (Audit log) của từng thiết bị. |
| **[`smart_light_app`](./smart_light_app)** | Flutter (Dart) | **Ứng dụng Di Động Đa Nền Tảng:**<br>• **Provisioning qua Bluetooth:** Tích hợp **Nordic nRF Mesh SDK** quét thiết bị chưa cấu hình, cấp phát khóa mạng (NetKey, AppKey) và phân phối địa chỉ Unicast trực tiếp cho Node và Gateway.<br>• **Quy trình Onboarding thiết bị từng bước (Step-by-step):** Giao diện hướng dẫn trực quan luồng nạp Gateway (cấu hình Wi-Fi SSID/Password) và nạp đèn.<br>• **Quản lý không gian (Rooms Management):** Gom nhóm và quản lý các thiết bị chiếu sáng theo từng phòng (phòng khách, phòng ngủ...).<br>• **Giám sát & Thống kê trực quan:** Theo dõi trạng thái bật/tắt, cường độ sáng và dòng tải theo thời gian thực; hiển thị biểu đồ lịch sử thống kê điện năng.<br>• **Nhật ký sự kiện & Cảnh báo an toàn:** Xem lịch sử bật/tắt (phân biệt rõ do người dùng hay do sự cố) và hiển thị banner cảnh báo rò điện tức thời. |

---

## ✨ Tính năng cốt lõi

* **Mở rộng linh hoạt với BLE Mesh:** Các nút đèn tự động kết nối mạng lưới đa điểm, chuyển tiếp bản tin qua nhiều chặng mà không gây tải cho bộ định tuyến Wi-Fi gia đình.
* **Cầu nối giao thức bất đối xứng (Protocol Bridging):** Gateway tự động ánh xạ giữa định dạng dữ liệu JSON qua MQTT và các gói tin nhị phân BLE Mesh, điều phối qua hệ điều hành thời gian thực FreeRTOS.
* **Bảo vệ an toàn điện chủ động tại biên (Edge Fail-safe):** Node đèn đo lường và tính toán dòng rò sai biệt trực tiếp tại biên; tự động ngắt relay bảo vệ độc lập ngay cả khi mất toàn bộ kết nối mạng Internet.
* **Cấu hình & Provisioning trực tiếp từ Mobile:** Quét và phân phối cấu hình khóa mạng (NetKey, AppKey, gán địa chỉ Unicast) trực tiếp qua Bluetooth điện thoại bằng chuẩn nRF Mesh.
* **Đồng bộ dữ liệu thời gian thực:** Lưu trữ lịch sử hoạt động, trạng thái bật/tắt và giám sát cường độ dòng điện qua nền tảng đám mây.

---

## 🛠 Công nghệ sử dụng

* **Firmware / Embedded:** ESP-IDF (C/C++), FreeRTOS, BLE Mesh SIG Models, ESP32-WROOM-32, ESP32-S3.
* **Phần cứng:** Module nguồn hạ áp AC-DC, Module Relay đóng ngắt tải, Biến dòng ZMCT103C, Quang trở LDR, Kit Yolo UNO, ESP32.
* **Backend:** Node.js, Docker, HiveMQ Cloud (MQTT TLS), Firebase Firestore, Google Compute Engine.
* **Mobile App:** Flutter, Nordic nRF Mesh Native SDK, Firebase Authentication.
