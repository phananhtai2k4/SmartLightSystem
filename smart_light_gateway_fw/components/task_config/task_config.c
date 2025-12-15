#include "task_config.h"

const task_config_t app_manager_task_config = {
    .name = TASK_NAME_APP,
    .stack_size = TASK_STACK_SIZE_APP,
    .priority = PRIORITY_APP,
    .core_id = CORE_ID_APP,
};

const task_config_t app_logic_task_config = {
    .name = TASK_NAME_APP_TASK,
    .stack_size = TASK_STACK_SIZE_APP_TASK,
    .priority = PRIORITY_APP_TASK,
    .core_id = CORE_ID_APP_TASK,
};