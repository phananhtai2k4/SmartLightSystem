idf_component_register(SRCS "device_control.c" "mqtt_service.c" "udp_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES sys_services esp_wifi esp_system esp_event json mqtt)