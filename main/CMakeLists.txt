set(srcs        "main.c"
                "app_ble_cmd.c"
                "app_door.c"
                "app_net.c"
                "app_ota.c"
                "app_supervisor.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES settings motor ble_gdp net_wifi ha_mqtt status_led
                                     esp_driver_gpio
                                     esp_timer esp_hw_support esp_system app_update esp_app_format)
