#ifndef __APP_SUPERVISOR_H_
#define __APP_SUPERVISOR_H_

#include "esp_err.h"

/** @brief Watchdog, idle BLE disconnect, periodic reboot and deferred restarts. */
esp_err_t app_supervisor_init(void);

/** @brief Restart cleanly from the supervisor task (safe to call from any callback). */
void app_supervisor_request_restart(void);

#endif
