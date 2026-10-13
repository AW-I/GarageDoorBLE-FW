idf_component_register(SRCS "motor.c" "motor_proto.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES esp_driver_uart esp_timer)
