#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "d_ble_queue.h"

#define QUEUE_SIZE 3

QueueHandle_t rx_queue = NULL;
QueueHandle_t tx_queue = NULL;

esp_err_t ble_queue_init(void) {
    if(rx_queue == NULL && tx_queue == NULL) {
        rx_queue = xQueueCreate( QUEUE_SIZE, sizeof( xMessage ) );
        tx_queue = xQueueCreate( QUEUE_SIZE, sizeof( xMessage ) );
        if(rx_queue == NULL || tx_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

esp_err_t ble_queue_rcv_push(uint8_t * rcvBuffer, uint32_t dataSize) {
    static xMessage buffer = {0};
    if(rx_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if(dataSize > QUEUE_BUFF_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer.len = dataSize;
    memcpy(buffer.data,rcvBuffer,dataSize);
    if(xQueueSend(rx_queue,&buffer,0) == pdPASS){
        return ESP_OK;
    }
    return ESP_FAIL;
}

uint32_t ble_queue_rcv(uint8_t * rxBuffer, uint32_t maxDataSize,uint32_t timeout){
    uint32_t rxSize = 0;
    static xMessage buffer = {0};
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

esp_err_t ble_queue_send_push(uint8_t * txBuffer, uint32_t dataSize) {
    static xMessage buffer = {0};
    if(tx_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if(dataSize > QUEUE_BUFF_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer.len = dataSize;
    memcpy(buffer.data,txBuffer,dataSize);
    if(xQueueSend(tx_queue,&buffer,0) == pdPASS){
        return ESP_OK;
    }
    return ESP_FAIL;
}

uint32_t ble_queue_send(uint8_t * txBuffer, uint32_t maxDataSize,uint32_t timeout){
    uint32_t rxSize = 0;
    static xMessage buffer = {0};
    if(rx_queue != NULL) {
        if( xQueueReceive(tx_queue, &buffer, (TickType_t)timeout ) == pdPASS ){
          //Copy to buffer
          if(buffer.len > maxDataSize) {
              return ESP_ERR_NO_MEM;
          } 
          rxSize = buffer.len;
          memcpy(txBuffer,&buffer.data,buffer.len);
      }
    }
    return rxSize;
}