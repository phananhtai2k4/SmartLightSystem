idf_component_register(SRCS "board_led.c" "board_button.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver sys_services ble_mesh_core app_logic)