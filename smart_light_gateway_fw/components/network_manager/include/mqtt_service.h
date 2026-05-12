#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>
#include "sys_nvs.h"

extern mqtt_config_t mqtt_config;
extern bool mqtt_connected;

// Định nghĩa kiểu con trỏ hàm callback cho dữ liệu nhận được
typedef void (*mqtt_data_callback_t)(const char *topic, int topic_len, const char *data, int data_len);
/**
 * @brief Khởi tạo và bắt đầu dịch vụ MQTT.
 *
 * Hàm này sẽ cấu hình, khởi tạo và khởi chạy client MQTT.
 * Nó đóng gói tất cả các chi tiết cấu hình bên trong.
 *
 * @return esp_err_t ESP_OK nếu thành công, ngược lại là mã lỗi.
 */
esp_err_t mqtt_service_init_and_start(void);

/**
 * @brief Dừng và giải phóng tài nguyên của dịch vụ MQTT.
 *
 * Hàm này sẽ dừng client MQTT, giải phóng bộ nhớ và các tài nguyên liên quan.
 * Nó cũng sẽ xóa mutex bảo vệ danh sách subscriptions.
 *
 * @return esp_err_t ESP_OK nếu thành công, ngược lại là mã lỗi.
 */
esp_err_t mqtt_service_stop_and_deinit(void);

/**
 * @brief Đăng ký một topic với một hàm callback.
 *
 * Khi có dữ liệu đến topic này, hàm callback tương ứng sẽ được gọi.
 * Cơ chế này bền bỉ, tự động đăng ký lại khi kết nối lại.
 *
 * @param topic Chuỗi topic cần đăng ký.
 * @param callback Hàm sẽ được gọi khi có dữ liệu mới.
 * @param qos Mức chất lượng dịch vụ (QoS).
 * @return esp_err_t ESP_OK nếu thành công, ngược lại là mã lỗi.
 */
esp_err_t mqtt_service_subscribe(const char *topic, mqtt_data_callback_t callback, int qos);

/**
 * @brief Gửi dữ liệu đến một topic.
 *
 * @param topic Chuỗi topic cần gửi đến.
 * @param data Dữ liệu cần gửi.
 * @param len Độ dài của dữ liệu.
 * @param qos Mức chất lượng dịch vụ (QoS).
 * @param retain Cờ retain.
 * @return int msg_id của tin nhắn đã gửi, hoặc -1 nếu thất bại.
 */
int mqtt_service_publish(const char *topic, const char *data, int len, int qos, int retain);

/**
 * @brief Kiểm tra trạng thái kết nối của MQTT.
 *
 * @return true nếu đã kết nối, false nếu ngược lại.
 */
bool mqtt_service_mqtt_connected(void);

/**
 * @brief Đăng ký lại tất cả các topic đã đăng ký trước đó.
 *
 * Hàm này sẽ được gọi khi kết nối lại với broker MQTT.
 * Nó sẽ đảm bảo rằng tất cả các topic đã đăng ký trước đó vẫn hoạt động.
 */
void resubscribe_all_topics(void);

#endif // MQTT_SERVICE_H