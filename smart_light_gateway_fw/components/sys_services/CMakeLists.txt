idf_component_register(SRCS "sys_nvs.c" "sys_event_queue.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver nvs_flash ble_mesh_core bt)