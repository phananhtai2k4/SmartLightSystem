idf_component_register(SRCS "time_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "driver" "esp_timer" "sys_services")