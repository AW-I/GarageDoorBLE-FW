set(srcs        "Core/main.c"
                "Driver/d_config.c"
                "Driver/d_ble_queue.c"
                "Driver/d_ble.c"
                "Driver/d_ble_gatt.c"
                "Driver/d_ble_gap.c"
                "Middleware/m_ble_api.c"
                "Middleware/m_door_api.c"
                "Middleware/m_system_api.c"
                "Application/a_task_handler.c"
                "Application/a_door_task.c"
                "Application/a_watchdog_task.c"
                )

set(incs        "Core/Inc"
                "Application/Inc"
                "Middleware/Inc"
                "Driver/Inc")

set(embed       "Embed/public_key.pem"
                "Embed/root.pem")     

idf_component_register( SRCS "${srcs}"
                        INCLUDE_DIRS "${incs}"
                        EMBED_FILES "${embed}")

