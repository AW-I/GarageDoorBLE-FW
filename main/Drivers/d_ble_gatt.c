#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "d_config.h"
#include "d_ble_gap.h"
#include "d_ble_gatt.h"
#include "d_ble_queue.h"

#define BLE_TIMEOUT 10

static const char* TAG = "BLE_GATT";

static uint16_t conn_id = 0xffff;
static esp_gatt_if_t gdp_gatts_if = 0xff;

bool is_shutdown = false;
bool is_connected = false;
uint32_t connection_time = 0;
static esp_bd_addr_t remote_bda = {0x0,};
static uint16_t ble_handle_table[GDP_IDX_NB];
static uint8_t txBuffer[QUEUE_BUFF_SIZE] = {0};

static uint8_t find_char_and_desr_index(uint16_t handle){
    uint8_t error = 0xff;
    for(int i = 0; i < GDP_IDX_NB ; i++){
        if(handle == ble_handle_table[i]){
            return i;
        }
    }
    return error;
}

esp_err_t gatts_close(bool shutdown) {
	is_shutdown = shutdown;
	if(is_connected){
		return esp_ble_gatts_close(gdp_gatts_if,conn_id);
	}
	return ESP_OK;
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *) param;
    uint8_t res = 0xff;
	uint8_t * nameBuff = NULL;
	uint8_t nameBuffLen = 0; 
    switch (event) {
    	case ESP_GATTS_REG_EVT:
		    ESP_LOGD(TAG, "ESP_GATTS_REG_EVT");
			//Get the name from config. 
			nameBuffLen = config_get_name(&nameBuff);
			if(nameBuffLen > 0 && nameBuff != NULL) {
				esp_ble_gap_set_device_name((char *)nameBuff);
    		} 
			esp_ble_gap_config_local_privacy(true);
        	esp_ble_gatts_create_attr_tab(gdp_gatt_db, gatts_if, GDP_IDX_NB, BLE_SVC_INST_ID);
       	break;
    	case ESP_GATTS_READ_EVT:
		    ESP_LOGD(TAG, "ESP_GATTS_READ_EVT");
            res = find_char_and_desr_index(p_data->read.handle);
			if(res == GDP_IDX_DATA_RECV_VAL) {
				static esp_gatt_rsp_t rsp;
				//Receive tx command
				uint32_t recSize = ble_queue_send(txBuffer,sizeof(txBuffer),BLE_TIMEOUT);
				memset(&rsp,0,sizeof(rsp));
				//If theres not a current message try to use existing.
				if(!recSize){
					recSize = strlen((char *)txBuffer);
					if(!recSize || recSize > sizeof(txBuffer)) {
						recSize = 0;
					}
				}
				//Set response and send
				rsp.attr_value.handle = param->read.handle;
				rsp.attr_value.len = recSize;
				memcpy(rsp.attr_value.value, txBuffer,recSize);
				esp_ble_gatts_send_response(gatts_if,p_data->read.conn_id,p_data->read.trans_id, ESP_GATT_OK, &rsp);
			}
       	 break;
    	case ESP_GATTS_WRITE_EVT: {
			ESP_LOGD(TAG, "ESP_GATTS_WRITE_EVT");
    	    res = find_char_and_desr_index(p_data->write.handle);
            if(p_data->write.is_prep == false){
				if(res == GDP_IDX_DATA_RECV_VAL){
					connection_time = xTaskGetTickCount();
					ESP_LOGD(TAG, "ESP_GATTS_WRITE_EVT : RX Len = %d\n", p_data->write.len);
					//Clear the TX Buffer
					memset(txBuffer,0,sizeof(txBuffer));
					esp_err_t err = ble_queue_rcv_push(p_data->write.value,p_data->write.len);
					if(err) {
						ESP_LOGD(TAG, "ESP_GATTS_WRITE_EVT : Failed to write to queue!");
					}
                }
            }
      	 	break;
    	}
    	case ESP_GATTS_MTU_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_MTU_EVT: MTU %d",p_data->mtu.mtu);
    	    break;
    	case ESP_GATTS_CONF_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_CONF_EVT");
    	    break;
    	case ESP_GATTS_UNREG_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_UNREG_EVT");
        	break;
    	case ESP_GATTS_DELETE_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_DELETE_EVT");
        	break;
    	case ESP_GATTS_START_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_START_EVT");
        	break;
    	case ESP_GATTS_STOP_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_STOP_EVT");
        	break;
    	case ESP_GATTS_CONNECT_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_CONNECT_EVT:");
    	    conn_id = p_data->connect.conn_id;
    	    gdp_gatts_if = gatts_if;
    	    is_connected = true;
			connection_time = xTaskGetTickCount();
    	    memcpy(&remote_bda,&p_data->connect.remote_bda,sizeof(esp_bd_addr_t));
			ESP_LOGI(TAG, "%02x:%02x:%02x:%02x:%02x:%02x",
			p_data->connect.remote_bda[0],
			p_data->connect.remote_bda[1],
			p_data->connect.remote_bda[2],
			p_data->connect.remote_bda[3],
			p_data->connect.remote_bda[4],
			p_data->connect.remote_bda[5]);
			esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
        	break;
    	case ESP_GATTS_DISCONNECT_EVT:
			ESP_LOGI(TAG, "Device disconnected.");
    	    is_connected = false;
			connection_time = 0;
			if(!is_shutdown) {
				esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&ble_adv_params);
			}
    	    break;
    	case ESP_GATTS_OPEN_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_OPEN_EVT");
    	    break;
    	case ESP_GATTS_CANCEL_OPEN_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_CANCEL_OPEN_EVT");
    	    break;
    	case ESP_GATTS_CLOSE_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_CLOSE_EVT");
    	    break;
    	case ESP_GATTS_LISTEN_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_LISTEN_EVT");
    	    break;
    	case ESP_GATTS_CONGEST_EVT:
			ESP_LOGD(TAG, "ESP_GATTS_CONGEST_EVT");
    	    break;
    	case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
    	    ESP_LOGD(TAG, "ESP_GATTS_CREAT_ATTR_TAB_EVT: The number handle =%x",param->add_attr_tab.num_handle);
    	    if (param->add_attr_tab.status != ESP_GATT_OK){
    	        ESP_LOGE(TAG, "Create attribute table failed, error code=0x%x", param->add_attr_tab.status);
    	    }
    	    else if (param->add_attr_tab.num_handle != GDP_IDX_NB){
    	        ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, GDP_IDX_NB);
    	    }
    	    else {
    	        memcpy(ble_handle_table, param->add_attr_tab.handles, sizeof(ble_handle_table));
    	        esp_ble_gatts_start_service(ble_handle_table[GDP_IDX_SVC]);
    	    }
    	    break;
    	}
    	default:
			ESP_LOGD(TAG, "Unhandled GATT Event = %x",event);
    	    break;
    }
}
