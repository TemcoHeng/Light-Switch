/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/* Attention! 
*  To maintain compliance with Nordic Semiconductor ASA�s Bluetooth profile 
*  qualification listings, this section of source code must not be modified.
*/

#include "ble_temperature.h"
#include <string.h>
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "app_util.h"


#define INVALID_TEMPERATURE_LEVEL 	255
#define TEMPERATURE_VALUE_LEN		2

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_temp       Temperature Service structure.
 * @param[in]   p_ble_evt    Event received from the BLE stack.
 */
static void on_connect(ble_temp_t * p_temp, ble_evt_t * p_ble_evt)
{
    p_temp->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_temp       Temperature Service structure.
 * @param[in]   p_ble_evt    Event received from the BLE stack.
 */
static void on_disconnect(ble_temp_t * p_temp, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_temp->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_temp       Temperature Service structure.
 * @param[in]   p_ble_evt    Event received from the BLE stack.
 */
static void on_write(ble_temp_t * p_temp, ble_evt_t * p_ble_evt)
{
    if (p_temp->is_notification_supported)
    {
        ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

        if ((p_evt_write->handle == p_temp->temperature_level_handles.cccd_handle)&&(p_evt_write->len == 2))          
        {
            // CCCD written, call application event handler
            if (p_temp->evt_handler != NULL)
            {
                ble_temp_evt_t evt;

                if (ble_srv_is_notification_enabled(p_evt_write->data))
                {
                    evt.evt_type = BLE_TEMP_EVT_NOTIFICATION_ENABLED;
                }
                else
                {
                    evt.evt_type = BLE_TEMP_EVT_NOTIFICATION_DISABLED;
                }

                p_temp->evt_handler(p_temp, &evt);
            }
        }
    }
}


void ble_temp_on_ble_evt(ble_temp_t * p_temp, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_temp, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_temp, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_temp, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for adding the Temperature Level characteristic.
 *
 * @param[in]   p_temp        Temperature Service structure.
 * @param[in]   p_temp_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t temperature_level_char_add(ble_temp_t * p_temp, const ble_temp_init_t * p_temp_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_Temperature_level;
    uint8_t             encoded_report_ref[BLE_SRV_ENCODED_REPORT_REF_LEN];
    uint8_t             init_len;

    // Add Temperature Level characteristic
    if (p_temp->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));

        // According to BAS_SPEC_V10, the read operation on cccd should be possible without
        // authentication.
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_temp_init->temperature_level_char_attr_md.cccd_write_perm;
        cccd_md.vloc       = BLE_GATTS_VLOC_STACK;
    }

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 1;
    char_md.char_props.notify = (p_temp->is_notification_supported) ? 1 : 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = (p_temp->is_notification_supported) ? &cccd_md : NULL;
    char_md.p_sccd_md         = NULL;

    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_TEMPERATURE_TYPE_CHAR);

    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_temp_init->temperature_level_char_attr_md.read_perm;
    attr_md.write_perm = p_temp_init->temperature_level_char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;

    initial_Temperature_level = p_temp_init->initial_temp_level;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint16_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = sizeof(uint16_t);
    attr_char_value.p_value   = &initial_Temperature_level;

    err_code = sd_ble_gatts_characteristic_add(p_temp->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_temp->temperature_level_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_temp_init->p_report_ref != NULL)
    {
        // Add Report Reference descriptor
        BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_REPORT_REF_DESCR);

        memset(&attr_md, 0, sizeof(attr_md));

        attr_md.read_perm = p_temp_init->temperature_level_report_read_perm;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

        attr_md.vloc    = BLE_GATTS_VLOC_STACK;
        attr_md.rd_auth = 0;
        attr_md.wr_auth = 0;
        attr_md.vlen    = 0;
        
        init_len = ble_srv_report_ref_encode(encoded_report_ref, p_temp_init->p_report_ref);
        
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        attr_char_value.p_uuid    = &ble_uuid;
        attr_char_value.p_attr_md = &attr_md;
        attr_char_value.init_len  = init_len;
        attr_char_value.init_offs = 0;
        attr_char_value.max_len   = attr_char_value.init_len;
        attr_char_value.p_value   = encoded_report_ref;

        err_code = sd_ble_gatts_descriptor_add(p_temp->temperature_level_handles.value_handle,
                                               &attr_char_value,
                                               &p_temp->report_ref_handle);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        p_temp->report_ref_handle = BLE_GATT_HANDLE_INVALID;
    }

    return NRF_SUCCESS;
}


uint32_t ble_temp_init(ble_temp_t * p_temp, const ble_temp_init_t * p_temp_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

    // Initialize service structure
    p_temp->evt_handler               = p_temp_init->evt_handler;
    p_temp->conn_handle               = BLE_CONN_HANDLE_INVALID;
    p_temp->is_notification_supported = p_temp_init->support_notification;
    p_temp->temperature_level_last    = INVALID_TEMPERATURE_LEVEL;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_TEMPERATURE_SERVICE);

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_temp->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Temperature level characteristic
    return temperature_level_char_add(p_temp, p_temp_init);
}


uint32_t ble_temp_Temperature_level_update(ble_temp_t * p_temp, uint16_t temperature_level)
{
    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;
	uint8_t temperature[TEMPERATURE_VALUE_LEN];
		
	
    if (temperature_level != p_temp->temperature_level_last)
    {
		temperature[0] = (uint8_t)(temperature_level >> 8);
		temperature[1] = (uint8_t)temperature_level ;
		
		// Initialize value struct.
        memset(&gatts_value, 0, sizeof(gatts_value));

        gatts_value.len     = TEMPERATURE_VALUE_LEN;
        gatts_value.offset  = 0;
        gatts_value.p_value = temperature;

        // Save new Temperature value.
        p_temp->temperature_level_last = temperature_level;

        // Update database.
        err_code = sd_ble_gatts_value_set(p_temp->conn_handle,
                                          p_temp->temperature_level_handles.value_handle,
                                          &gatts_value);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }

        // Send value if connected and notifying.
        if ((p_temp->conn_handle != BLE_CONN_HANDLE_INVALID) && p_temp->is_notification_supported)
        {
            ble_gatts_hvx_params_t hvx_params;

            memset(&hvx_params, 0, sizeof(hvx_params));

            hvx_params.handle = p_temp->temperature_level_handles.value_handle;
            hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
            hvx_params.offset = gatts_value.offset;
            hvx_params.p_len  = &gatts_value.len;
            hvx_params.p_data = gatts_value.p_value;

            err_code = sd_ble_gatts_hvx(p_temp->conn_handle, &hvx_params);
        }
        else
        {
            err_code = NRF_ERROR_INVALID_STATE;
        }
    }

    return err_code;
}
