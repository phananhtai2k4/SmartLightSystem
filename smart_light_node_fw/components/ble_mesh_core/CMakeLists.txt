idf_component_register(SRCS "ble_mesh_init.c" "ble_mesh_composition.c" "ble_mesh_callbacks.c" "bluetooth_init.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver sys_services app_logic nvs_flash bt freertos device_identity)