#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "d_ble_queue.h"

#define RX_QUEUE_SIZE 1

QueueHandle_t rx_queue = NULL;
static xMessage buffer;

esp_err_t ble_queue_init(void) {
    if(rx_queue == NULL) {
        rx_queue = xQueueCreate( 1, sizeof( xMessage ) );
        if(rx_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

/**ble_queue_rcv_push
 * @brief Push a received message to the queue.
 * 
 * @param rcvBuffer Receive buffer for the message.
 * @param dataSize 
 * @return esp_err_t OK if success, any other value if failed.
 */

esp_err_t ble_queue_rcv_push(uint8_t * rcvBuffer, uint32_t dataSize) {
    if(rx_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if(dataSize > ESP_ERR_INVALID_ARG) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer.len = dataSize;
    memcpy(buffer.data,rcvBuffer,dataSize);
    if(xQueueSend(rx_queue,&buffer,0) == pdPASS){
        return ESP_OK;
    }
    return ESP_FAIL;
}

/** ble_queue_rcv
 * @brief 
 * 
 * @param rxBuffer 
 * @param maxDataSize 
 * @return uint32_t size of the received message, 0 if nothing received.
 */
uint32_t ble_queue_rcv(uint8_t * rxBuffer, uint32_t maxDataSize,uint32_t timeout){
    uint32_t rxSize = 0;
    if(rx_queue != NULL) {
        if( xQueueReceive(rx_queue, &buffer, (TickType_t)timeout ) == pdPASS ){
          //Copy to buffer
          if(buffer.len > maxDataSize) {
              return ESP_ERR_NO_MEM;
          } 
          rxSize = buffer.len;
          memcpy(rxBuffer,&buffer.data,buffer.len);
      }
    }
    return rxSize;
}