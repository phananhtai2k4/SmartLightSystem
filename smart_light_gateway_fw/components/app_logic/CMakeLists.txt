idf_component_register(SRCS "app_task.c" "app_event_handler.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver sys_services task_config network_manager wifi_manager board_hal time_manager)