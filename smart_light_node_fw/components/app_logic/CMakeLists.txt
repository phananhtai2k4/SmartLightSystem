idf_component_register(SRCS "app_event_handler.c" "app_task.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver sys_services ble_mesh_core nvs_flash bt board_hal)