idf_component_register(SRCS "board_adc_continuous.c" "board_adc.c" "board_led.c" "board_button.c" "board_hal.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver sys_services ble_mesh_core app_logic esp_adc)