#ifndef __D_BLE_QUEUE_H_
#define __D_BLE_QUEUE_H_

#include "esp_err.h"

#define RX_QUEUE_BUFF_SIZE 64

typedef struct {
    uint8_t len;
    uint8_t data[ RX_QUEUE_BUFF_SIZE ];
} xMessage;

/** ble_queue_init
 * @brief Initialize the ble queues. 
 * 
 * @return esp_err_t OK if success, any other value if failed.
 */
esp_err_t ble_queue_init(void);

/**ble_queue_rcv_push
 * @brief Push a received message to the queue.
 * 
 * @param rcvBuffer Receive buffer for the message.
 * @param dataSize 
 * @return esp_err_t OK if success, any other value if failed.
 */

esp_err_t ble_queue_rcv_push(uint8_t * rcvBuffer, uint32_t dataSize);

/** ble_queue_rcv
 * @brief 
 * 
 * @param rxBuffer 
 * @param maxDataSize 
 * @return uint32_t size of the received message, 0 if nothing received.
 */
uint32_t ble_queue_rcv(uint8_t * rxBuffer, uint32_t maxDataSize,uint32_t timeout);

#endif