set(srcs    "Core/main.c"
            "Application/a_task_handler.c"
            "Application/a_door_task.c"
            "Application/a_watchdog_task.c"
            "Middlewares/m_ble_api.c"
            "Middlewares/m_door_api.c"
            "Drivers/d_ble_queue.c"
            "Drivers/d_ble.c"
            "Drivers/d_ble_gatt.c"
            "Drivers/d_ble_gap.c"
            "Drivers/d_config.c")

set(incs    "Core/Inc"
            "Application/Inc"
            "Middlewares/Inc"
            "Drivers/Inc")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "${incs}")