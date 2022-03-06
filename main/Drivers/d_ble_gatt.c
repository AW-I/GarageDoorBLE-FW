#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_log.h"

#include "d_ble_gap.h"
#include "d_ble_gatt.h"
#include "d_ble_queue.h"

static const char* TAG = "BLE_GATT";

static uint16_t spp_mtu_size = 23;
static uint16_t spp_conn_id = 0xffff;
static esp_gatt_if_t spp_gatts_if = 0xff;

static bool enable_data_ntf = false;
static bool is_connected = false;
static esp_bd_addr_t spp_remote_bda = {0x0,};
static uint16_t ble_handle_table[GDP_IDX_NB];

static uint8_t find_char_and_desr_index(uint16_t handle)
{
    uint8_t error = 0xff;
    for(int i = 0; i < GDP_IDX_NB ; i++){
        if(handle == ble_handle_table[i]){
            return i;
        }
    }
    return error;
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_ble_gatts_cb_param_t *p_data = (esp_ble_gatts_cb_param_t *) param;
    uint8_t res = 0xff;
    ESP_LOGI(TAG, "event = %x\n",event);
    switch (event) {
    	case ESP_GATTS_REG_EVT:
    	    ESP_LOGI(TAG, "ESP_GATTS_REG_EVT\n");
        	esp_ble_gap_set_device_name(DEVICE_NAME);
			esp_ble_gap_config_adv_data((esp_ble_adv_data_t *)&ble_adv_data);
        	esp_ble_gatts_create_attr_tab(gdp_gatt_db, gatts_if, GDP_IDX_NB, BLE_SVC_INST_ID);
       	break;
    	case ESP_GATTS_READ_EVT:
            res = find_char_and_desr_index(p_data->read.handle);
       	 break;
    	case ESP_GATTS_WRITE_EVT: {
    	    res = find_char_and_desr_index(p_data->write.handle);
            if(p_data->write.is_prep == false){
                ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT : handle = %d\n", res);
                if(res == GDP_IDX_DATA_NTF_CFG){
                    if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x01)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = true;
                    }else if((p_data->write.len == 2)&&(p_data->write.value[0] == 0x00)&&(p_data->write.value[1] == 0x00)){
                        enable_data_ntf = false;
                    }
                }
                else if(res == GDP_IDX_DATA_RECV_VAL){
					ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT : RX Len = %d\n", p_data->write.len);
					esp_err_t err = ble_queue_rcv_push(p_data->write.value + 1,p_data->write.len);
					if(err) {
						ESP_LOGI(TAG, "ESP_GATTS_WRITE_EVT : Failed to write to queue!");
					}
                }
            }else if((p_data->write.is_prep == true)&&(res == GDP_IDX_DATA_RECV_VAL)){
                ESP_LOGI(TAG, "ESP_GATTS_PREP_WRITE_EVT : handle = %d\n", res);
            }
      	 	break;
    	}
    	case ESP_GATTS_EXEC_WRITE_EVT:{
    	    ESP_LOGI(TAG, "ESP_GATTS_EXEC_WRITE_EVT\n");
    	    if(p_data->exec_write.exec_write_flag){
    	    }
    	    break;
    	}
    	case ESP_GATTS_MTU_EVT:
    	    spp_mtu_size = p_data->mtu.mtu;
    	    break;
    	case ESP_GATTS_CONF_EVT:
    	    break;
    	case ESP_GATTS_UNREG_EVT:
        	break;
    	case ESP_GATTS_DELETE_EVT:
        	break;
    	case ESP_GATTS_START_EVT:
        	break;
    	case ESP_GATTS_STOP_EVT:
        	break;
    	case ESP_GATTS_CONNECT_EVT:
    	    spp_conn_id = p_data->connect.conn_id;
    	    spp_gatts_if = gatts_if;
    	    is_connected = true;
    	    memcpy(&spp_remote_bda,&p_data->connect.remote_bda,sizeof(esp_bd_addr_t));
        	break;
    	case ESP_GATTS_DISCONNECT_EVT:
    	    is_connected = false;
    	    enable_data_ntf = false;
    	    esp_ble_gap_start_advertising((esp_ble_adv_params_t *)&ble_adv_params);
    	    break;
    	case ESP_GATTS_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CANCEL_OPEN_EVT:
    	    break;
    	case ESP_GATTS_CLOSE_EVT:
    	    break;
    	case ESP_GATTS_LISTEN_EVT:
    	    break;
    	case ESP_GATTS_CONGEST_EVT:
    	    break;
    	case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
    	    ESP_LOGI(TAG, "The number handle =%x\n",param->add_attr_tab.num_handle);
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
    	    break;
    }
}
