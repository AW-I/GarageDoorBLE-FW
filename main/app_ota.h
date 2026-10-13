#ifndef __APP_OTA_H_
#define __APP_OTA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/*
 * Firmware update from the phone app.
 *
 *  1. OTA_BEGIN <size>     (command, after AUTH) opens the inactive OTA slot. Sectors are
 *                          erased as the image arrives, so this replies straight away.
 *  2. Data characteristic  <offset u32 LE><bytes>, any number of write-no-rsp chunks. A chunk
 *                          whose offset is not the next expected byte is dropped.
 *  3. OTA_STATUS           replies the next expected offset. The phone resends from there.
 *  4. OTA_END              verifies the image (hash, chip, project name), selects it for boot.
 *
 * The new image boots in pending-verify state and is rolled back on the next reset unless
 * app_ota_confirm() runs first (successful BLE AUTH, or stable uptime - see the supervisor).
 */

#define APP_OTA_CHUNK_HDR 4

/** @brief Log the running slot and remember whether it still needs confirming. */
esp_err_t app_ota_init(void);

/** @brief Mark the running image as good so the bootloader keeps it. No-op once confirmed. */
void app_ota_confirm(void);

/** @brief True while the running image has not been confirmed yet. */
bool app_ota_pending_confirm(void);

/** @brief Start receiving an image of size bytes into the inactive slot. Aborts any open update. */
esp_err_t app_ota_begin(uint32_t size);

/** @brief Data characteristic handler: <offset u32 LE><bytes>. Host task. */
int app_ota_on_data(const uint8_t *data, size_t len);

/** @brief Next expected offset, or the write error that ended the update. */
esp_err_t app_ota_status(uint32_t *received);

/** @brief Verify the complete image and boot it on the next restart. */
esp_err_t app_ota_finish(void);

/** @brief Drop an open update (disconnect, phone cancelled). */
void app_ota_abort(void);

#endif
