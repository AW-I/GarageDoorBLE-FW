#ifndef __D_BLE_QUEUE_H_
#define __D_BLE_QUEUE_H_

#include "esp_err.h"

#define QUEUE_BUFF_SIZE 64

typedef struct {
    uint8_t len;
    uint8_t data[ QUEUE_BUFF_SIZE ];
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
 * @param dataSize size of the message
 * @return esp_err_t OK if success, any other value if failed.
 */

esp_err_t ble_queue_rcv_push(uint8_t * rcvBuffer, uint32_t dataSize);

/** ble_queue_rcv
 * @brief Receive a message from ble queue.
 * 
 * @param rxBuffer pointer to the receive buffer.
 * @param maxDataSize Maximum size of the buffer
 * @return uint32_t size of the received message, 0 if nothing received.
 */
uint32_t ble_queue_rcv(uint8_t * rxBuffer, uint32_t maxDataSize,uint32_t timeout);

/**ble_queue_send_push
 * @brief Push a message to be sent to the queue.
 * 
 * @param txBuffer Tx buffer for the message.
 * @param dataSize size of the message
 * @return esp_err_t OK if success, any other value if failed.
 */
esp_err_t ble_queue_send_push(uint8_t * txBuffer, uint32_t dataSize);

/** ble_queue_send
 * @brief Get the message to send to the client.
 * 
 * @param txBuffer buffer to store message into.
 * @param maxDataSize maxmimum datasize to store. 
 * @param timeout timeout to receive message.
 * @return uint32_t Size of the receieved message.
 */
uint32_t ble_queue_send(uint8_t * txBuffer, uint32_t maxDataSize,uint32_t timeout);


#endif