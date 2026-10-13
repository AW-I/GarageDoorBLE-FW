#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "app_ota.h"

static const char *TAG = "OTA";

static SemaphoreHandle_t s_lock;
static bool s_pending_confirm;

/* Open update, guarded by s_lock (command task and NimBLE host task). */
static const esp_partition_t *s_part;
static esp_ota_handle_t s_handle;
static bool s_active;
static uint32_t s_size;
static uint32_t s_received;
static esp_err_t s_error;

/* Caller holds s_lock. */
static void close_update(void) {
	if (s_active) {
		esp_ota_abort(s_handle);
	}
	s_active = false;
	s_handle = 0;
	s_part = NULL;
}

esp_err_t app_ota_init(void) {
	s_lock = xSemaphoreCreateMutex();
	if (s_lock == NULL) {
		return ESP_ERR_NO_MEM;
	}
	const esp_partition_t *running = esp_ota_get_running_partition();
	esp_ota_img_states_t state;
	if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
		s_pending_confirm = true;
		ESP_LOGW(TAG, "Running new image from %s - waiting for confirmation.", running->label);
	} else {
		ESP_LOGI(TAG, "Running from %s.", running->label);
	}
	return ESP_OK;
}

void app_ota_confirm(void) {
	if (!s_pending_confirm) {
		return;
	}
	esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Confirming image failed: %s.", esp_err_to_name(err));
		return;
	}
	s_pending_confirm = false;
	ESP_LOGI(TAG, "New image confirmed, rollback cancelled.");
}

bool app_ota_pending_confirm(void) {
	return s_pending_confirm;
}

esp_err_t app_ota_begin(uint32_t size) {
	const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
	if (part == NULL) {
		ESP_LOGE(TAG, "No OTA slot - flash the OTA partition table over USB.");
		return ESP_ERR_NOT_FOUND;
	}
	if (size == 0 || size > part->size) {
		ESP_LOGE(TAG, "Image size %lu does not fit %s (%lu).", (unsigned long)size, part->label,
				 (unsigned long)part->size);
		return ESP_ERR_INVALID_SIZE;
	}

	xSemaphoreTake(s_lock, portMAX_DELAY);
	close_update();
	esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &s_handle);
	if (err == ESP_OK) {
		s_part = part;
		s_active = true;
		s_size = size;
		s_received = 0;
		s_error = ESP_OK;
		ESP_LOGI(TAG, "Receiving %lu bytes into %s.", (unsigned long)size, part->label);
	} else {
		ESP_LOGE(TAG, "OTA begin failed: %s.", esp_err_to_name(err));
	}
	xSemaphoreGive(s_lock);
	return err;
}

int app_ota_on_data(const uint8_t *data, size_t len) {
	if (len <= APP_OTA_CHUNK_HDR) {
		return -1;
	}
	uint32_t offset = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
					  ((uint32_t)data[3] << 24);
	const uint8_t *chunk = data + APP_OTA_CHUNK_HDR;
	size_t chunk_len = len - APP_OTA_CHUNK_HDR;
	int rc = 0;

	xSemaphoreTake(s_lock, portMAX_DELAY);
	if (!s_active || s_error != ESP_OK) {
		rc = -1;
	} else if (offset != s_received) {
		/* Lost or repeated chunk - the phone resends from OTA_STATUS. */
		ESP_LOGD(TAG, "Chunk at %lu, expected %lu.", (unsigned long)offset, (unsigned long)s_received);
		rc = -1;
	} else if (s_received + chunk_len > s_size) {
		s_error = ESP_ERR_INVALID_SIZE;
		rc = -1;
	} else {
		esp_err_t err = esp_ota_write(s_handle, chunk, chunk_len);
		if (err == ESP_OK) {
			s_received += chunk_len;
		} else {
			ESP_LOGE(TAG, "Write at %lu failed: %s.", (unsigned long)offset, esp_err_to_name(err));
			s_error = err;
			rc = -1;
		}
	}
	xSemaphoreGive(s_lock);
	return rc;
}

esp_err_t app_ota_status(uint32_t *received) {
	xSemaphoreTake(s_lock, portMAX_DELAY);
	esp_err_t err = !s_active ? ESP_ERR_INVALID_STATE : s_error;
	*received = s_received;
	xSemaphoreGive(s_lock);
	return err;
}

esp_err_t app_ota_finish(void) {
	xSemaphoreTake(s_lock, portMAX_DELAY);
	esp_err_t err = ESP_OK;
	const esp_partition_t *part = s_part;
	if (!s_active) {
		err = ESP_ERR_INVALID_STATE;
	} else if (s_error != ESP_OK) {
		err = s_error;
	} else if (s_received != s_size) {
		ESP_LOGE(TAG, "Image incomplete: %lu of %lu bytes.", (unsigned long)s_received,
				 (unsigned long)s_size);
		err = ESP_ERR_INVALID_SIZE;
	}
	if (err != ESP_OK) {
		close_update();
		xSemaphoreGive(s_lock);
		return err;
	}

	/* Checks the image header, chip and the SHA-256 appended to the image. Frees the handle. */
	err = esp_ota_end(s_handle);
	s_active = false;
	s_handle = 0;
	s_part = NULL;
	xSemaphoreGive(s_lock);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Image rejected: %s.", esp_err_to_name(err));
		return err;
	}

	/* A valid image of some other project would still brick the door - refuse it. */
	esp_app_desc_t desc;
	err = esp_ota_get_partition_description(part, &desc);
	if (err != ESP_OK) {
		return err;
	}
	if (strncmp(desc.project_name, esp_app_get_description()->project_name, sizeof(desc.project_name)) != 0) {
		ESP_LOGE(TAG, "Image is for \"%.32s\", not this controller.", desc.project_name);
		return ESP_ERR_OTA_VALIDATE_FAILED;
	}

	err = esp_ota_set_boot_partition(part);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Selecting %s failed: %s.", part->label, esp_err_to_name(err));
		return err;
	}
	ESP_LOGW(TAG, "Firmware %.32s ready in %s, boots on restart.", desc.version, part->label);
	return ESP_OK;
}

void app_ota_abort(void) {
	if (s_lock == NULL) {
		return;
	}
	xSemaphoreTake(s_lock, portMAX_DELAY);
	if (s_active) {
		ESP_LOGW(TAG, "Update aborted at %lu of %lu bytes.", (unsigned long)s_received, (unsigned long)s_size);
	}
	close_update();
	xSemaphoreGive(s_lock);
}
