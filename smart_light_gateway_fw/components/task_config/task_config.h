#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tên các task
#define TASK_NAME_APP       "app_manager"
#define TASK_NAME_APP_TASK  "app_task"
// Cấu hình cho từng task
#define TASK_STACK_SIZE_APP       (4096)
#define PRIORITY_APP              (7)
#define CORE_ID_APP               (1)

#define TASK_STACK_SIZE_APP_TASK (4096)
#define PRIORITY_APP_TASK        (20)
#define CORE_ID_APP_TASK         (1)
// Hoặc dùng struct nếu thích mở rộng:
typedef struct {
    const char *name;
    uint32_t stack_size;
    UBaseType_t priority;
    BaseType_t core_id;
} task_config_t;

extern const task_config_t app_manager_task_config;
extern const task_config_t app_logic_task_config;
