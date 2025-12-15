idf_component_register(SRCS "sys_event_queue.c" "sys_nvs.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver ble_mesh_core app_logic nvs_flash bt)