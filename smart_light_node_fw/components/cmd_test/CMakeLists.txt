idf_component_register(SRCS "cmd_test.c" "cmd_test_cmds.c"
                       INCLUDE_DIRS "include"
                       REQUIRES console ble_mesh_core)