// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bt.h"
#include "bta_api.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_main.h"

#define GATTS_SERVICE_UUID_TEST 	0xFFFF
#define GATTS_CHAR_UUID_TEST 		0xFF01
#define GATTS_DESCR_UUID_TEST 		0x3333
#define APP_ID_TEST					0x18
#define GATTS_NUM_HANDLE_TEST		4
#define TEST_DEVICE_NAME			"snakeNB"

#define TEST_MANUFACTURER_DATA_LEN	8
static uint16_t test_service_uuid = GATTS_NUM_HANDLE_TEST;
static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x1, 0x2, 0x1, 0x2, 0x1, 0x2, 0x1, 0x2};

static esp_ble_adv_data_t test_adv_data = {
	.set_scan_rsp = false,
	.include_name = true,
	.include_txpower = true,
	.min_interval = 0x20,
	.max_interval = 0x40,
	.appearance = 0,
	.manufacturer_len = TEST_MANUFACTURER_DATA_LEN,
	.p_manufacturer_data = &test_manufacturer[0],
	.service_data_len = 0,
	.p_service_data = NULL,
	.service_uuid_len = 2,
	.p_service_uuid = (uint8_t *)&test_service_uuid,
};

static esp_ble_adv_params_t test_adv_params = {
	.adv_int_min 		= 0x20,
	.adv_int_max		= 0x40,
	.adv_type			= ADV_TYPE_IND,
	.own_addr_type		= BLE_ADDR_TYPE_PUBLIC,
	//.peer_addr			= 
	//.peer_addr_type		=
	.channel_map 		= ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

struct gatts_test_inst {
	uint16_t gatt_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_handle;
	esp_gatt_srvc_id_t service_id;
	uint16_t char_handle;
	esp_bt_uuid_t char_uuid;
	esp_gatt_perm_t perm;
	esp_gatt_char_prop_t property;
	uint16_t descr_handle;
	esp_bt_uuid_t descr_uuid;
};
static struct gatts_test_inst gl_test;

static void gap_event_handler(uint32_t event, void *param)
{
	LOG_ERROR("GAP_EVT, event %d\n", event);
}

static void gatts_event_handler(uint32_t event, void *param)
{
	esp_ble_gatts_cb_param_t *p = (esp_ble_gatts_cb_param_t *)param;

	switch (event) {
	case ESP_GATTS_REG_EVT:
		LOG_ERROR("REGISTER_APP_EVT, status %d, gatt_if %d, app_id %d\n", p->reg.status, p->reg.gatt_if, p->reg.app_id);
		gl_test.gatt_if = p->reg.gatt_if;
		gl_test.service_id.is_primary = 1;
		gl_test.service_id.id.inst_id = 0x00;
		gl_test.service_id.id.uuid.len = ESP_UUID_LEN_16;
		gl_test.service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_TEST;
		esp_ble_gatts_create_service(gl_test.gatt_if, &gl_test.service_id, GATTS_NUM_HANDLE_TEST);

		esp_ble_gap_set_device_name(TEST_DEVICE_NAME);
		esp_ble_gap_config_adv_data(&test_adv_data);
		esp_ble_gap_start_advertising(&test_adv_params);
		break;
	case ESP_GATTS_READ_EVT: {
		LOG_ERROR("GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", p->read.conn_id, p->read.trans_id, p->read.handle);
		esp_gatt_rsp_t rsp;
		memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
		rsp.attr_value.handle = p->read.handle;
		rsp.attr_value.len = 4;
		rsp.attr_value.value[0] = 0xde;
        rsp.attr_value.value[1] = 0xed;
        rsp.attr_value.value[2] = 0xbe;
        rsp.attr_value.value[3] = 0xef;
		esp_ble_gatts_send_response(p->read.conn_id, p->read.trans_id,
              ESP_GATT_OK, &rsp);
		break;
	}
	case ESP_GATTS_WRITE_EVT:
	case ESP_GATTS_EXEC_WRITE_EVT:
	case ESP_GATTS_MTU_EVT:
	case ESP_GATTS_CONF_EVT:
	case ESP_GATTS_UNREG_EVT:
		break;
	case ESP_GATTS_CREATE_EVT:
		LOG_ERROR("CREATE_SERVICE_EVT, status %d, gatt_if %d,  service_handle %d\n", p->create.status, p->create.gatt_if, p->create.service_handle);
		gl_test.service_handle = p->create.service_handle;
		gl_test.char_uuid.len = ESP_UUID_LEN_16;
		gl_test.char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_TEST;

		esp_ble_gatts_start_service(gl_test.service_handle);

		esp_ble_gatts_add_char(gl_test.service_handle, &gl_test.char_uuid,
					ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
					ESP_GATT_CHAR_PROP_BIT_READ|ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_NOTIFY);
		break;
	case ESP_GATTS_ADD_INCL_SRVC_EVT:
		break;
	case ESP_GATTS_ADD_CHAR_EVT:
		LOG_ERROR("ADD_CHAR_EVT, status %d, gatt_if %d,  attr_handle %d, service_handle %d\n",
					p->add_char.status, p->add_char.gatt_if, p->add_char.attr_handle, p->add_char.service_handle);

		gl_test.char_handle = p->add_char.attr_handle;
		gl_test.descr_uuid.len = ESP_UUID_LEN_16;
		gl_test.descr_uuid.uuid.uuid16 = GATT_UUID_CHAR_CLIENT_CONFIG;
		esp_ble_gatts_add_char_descr(gl_test.service_handle, &gl_test.descr_uuid,
					ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE);
		break;
	case ESP_GATTS_ADD_CHAR_DESCR_EVT:
		LOG_ERROR("ADD_DESCR_EVT, status %d, gatt_if %d,  attr_handle %d, service_handle %d\n",
					p->add_char.status, p->add_char.gatt_if, p->add_char.attr_handle, p->add_char.service_handle);
		break;
	case ESP_GATTS_DELELTE_EVT:
		break;
	case ESP_GATTS_START_EVT:
		LOG_ERROR("SERVICE_START_EVT, status %d, gatt_if %d, service_handle %d\n",
					p->start.status, p->start.gatt_if, p->start.service_handle);
		break;
	case ESP_GATTS_STOP_EVT:
		break;
	case ESP_GATTS_CONNECT_EVT:
		LOG_ERROR("SERVICE_START_EVT, conn_id %d, gatt_if %d, remote %02x:%02x:%02x:%02x:%02x:%02x:, is_conn %d\n",
						p->connect.conn_id, p->connect.gatt_if,
						p->connect.remote_bda[0], p->connect.remote_bda[1], p->connect.remote_bda[2],
						p->connect.remote_bda[3], p->connect.remote_bda[4], p->connect.remote_bda[5],
						p->connect.is_connected);
		gl_test.conn_id = p->connect.conn_id;
		break;
	case ESP_GATTS_DISCONNECT_EVT:
	case ESP_GATTS_OPEN_EVT:
	case ESP_GATTS_CANCEL_OPEN_EVT:
	case ESP_GATTS_CLOSE_EVT:
	case ESP_GATTS_LISTEN_EVT:
	case ESP_GATTS_CONGEST_EVT:
	default:
		break;
	}
}

void app_main()
{
	esp_err_t ret;

    bt_controller_init();
	LOG_ERROR("%s init bluetooth\n", __func__);
    ret = esp_init_bluetooth();
	if (ret) {
		LOG_ERROR("%s init bluetooth failed\n", __func__);
		return;
	}
    ret = esp_enable_bluetooth();
	if (ret) {
		LOG_ERROR("%s enable bluetooth failed\n", __func__);
		return;
	}

	esp_ble_gatts_register_callback(gatts_event_handler);
	esp_ble_gap_register_callback(gap_event_handler);
	esp_ble_gatts_app_register(0x18);

	return;
}
