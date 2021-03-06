/**
 * Copyright (c) 2014 - 2018, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
 
#include <stdint.h>
#include <string.h>
#include "our_service.h"
#include "ble_srv_common.h"
#include "app_error.h"
#include "SEGGER_RTT.h"

/** @file
 *
 * @defgroup ble_sdk_app_template_main main.c
 * @{
 * @ingroup ble_sdk_app_template
 * @brief Template project main file.
 *
 * This file contains a template for creating a new application. It has the code necessary to wakeup
 * from button, advertise, get a connection restart advertising on disconnect and if no new
 * connection created go back to system-off mode.
 * It can easily be used as a starting point for creating a new application, the comments identified
 * with 'YOUR_JOB' indicates where and how you can customize.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "fds.h"
#include "peer_manager.h"
#include "bsp_btn_ble.h"
#include "sensorsim.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrfx_spim.h"
#include "nrf_delay.h"
#include "sans_serif_30x60.h"
#include "clock.h"


#define DEVICE_NAME                     "OurService"                            /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "NordicSemiconductor"                   /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                50                                      /**< The advertising interval (in units of 0.625 ms. This value corresponds to 187.5 ms). */

#define APP_ADV_DURATION                18000                                   /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(100, UNIT_1_25_MS)        /**< Minimum acceptable connection interval (0.1 seconds). */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(200, UNIT_1_25_MS)        /**< Maximum acceptable connection interval (0.2 second). */
#define SLAVE_LATENCY                   0                                       /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)         /**< Connection supervisory timeout (4 seconds). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                   /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                  /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND                  1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                       /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                  0                                       /**< LE Secure Connections not enabled. */
#define SEC_PARAM_KEYPRESS              0                                       /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    /**< No I/O capabilities. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */

#define DEAD_BEEF                       0xDEADBEEF                              /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */


NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;                        /**< Handle of the current connection. */

// STEP 1: Declare a ble_os_t service structure for our application
ble_os_t m_our_service;

APP_TIMER_DEF(m_our_char_timer_id);
#define OUR_CHAR_TIMER_INTERVAL         APP_TIMER_TICKS(1000) // 1000ms intervals

APP_TIMER_DEF(m_our_clock_timer_id);
#define OUR_CLOCK_TIMER_INTERVAL         APP_TIMER_TICKS(60000) // 1000ms intervals


// STEP 5: Declare variable holding our service UUID
static ble_uuid_t m_adv_uuids[] = 
{
    {
        BLE_UUID_OUR_SERVICE,
        BLE_UUID_TYPE_VENDOR_BEGIN
    }
};

#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define ST77XX_ORANGE 0xFC00

#define ST7789_CMD_SWRESET 0x01
#define ST7789_CMD_SLPOUT  0x11
#define ST7789_CMD_NORON   0x13
#define ST7789_CMD_INVON   0x21
#define ST7789_CMD_DISPON  0x29
#define ST7789_CMD_CASET   0x2A
#define ST7789_CMD_RASET   0x2B
#define ST7789_CMD_RAMWR   0x2C
#define ST7789_CMD_VSCRDEF 0x33
#define ST7789_CMD_COLMOD  0x3A
#define ST7789_CMD_MADCTL  0x36
#define ST7789_CMD_VSCSAD  0x37

#define BUFFER_SIZE 254

#define FRAMEBUFFER_WIDTH 240
#define FRAMEBUFFER_HEIGHT 320


//spi config
#define NRFX_SPIM_RESET_PIN  30
#define NRFX_SPIM_SCK_PIN     4
#define NRFX_SPIM_MOSI_PIN    3
#define NRFX_SPIM_MISO_PIN  255
#define NRFX_SPIM_SS_PIN     28
#define NRFX_SPIM_DCX_PIN    29

#define SPIM_INSTANCE 0
static nrfx_spim_t lcdSpi = NRFX_SPIM_INSTANCE(SPIM_INSTANCE); 
static nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG;
//nrfx_spim_xfer_desc_t xfer_desc;

static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

//commands
static uint8_t lcd_cmd_ramwr = ST7789_CMD_RAMWR;
static nrfx_spim_xfer_desc_t xfer_lcd_cmd_ramwr = NRFX_SPIM_XFER_TX(&lcd_cmd_ramwr, 1);

//clock config
static clock_position_t clock = {
    60,     //start x
    90,     //start y
    89,     //width
    149,    //height

    90,
    90,
    119,
    149,

    120,
    90,
    149,
    149,

    150,
    90,
    179,
    149
};

static clock_time_t clockTime = {
    0,
    0
};

void setWindow(uint8_t startX, uint8_t startY, uint8_t width, uint8_t height);
void drawNumber(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t textColor, uint16_t bgColor, uint8_t number);

static void advertising_start(bool erase_bonds);

static void timer_timeout_handler(void * p_context)
{
    //SEGGER_RTT_printf(0, "TICKTEST!");
    if(m_our_service.conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        int32_t temperature = 0;
        sd_temp_get(&temperature);
        //SEGGER_RTT_printf(0, "temp: %d\n", temperature);
        //SEGGER_RTT_printf(0, "current_temp: %d\n", m_our_service.current_temp);
        if(m_our_service.current_temp != temperature)
        {
            //SEGGER_RTT_printf(0, "changed temperature\n");
            our_temperature_characteristic_update(&m_our_service, &temperature);
            nrf_gpio_pin_toggle(20);
            m_our_service.current_temp = temperature;
        }
    }
}

static void clock_timer_handler(void * p_context)
{
    clockTime.minute++;

    if(clockTime.minute >= 60){
        clockTime.hour++;
        clockTime.minute = 0;

        

        if(clockTime.hour >= 24){
            clockTime.hour = 0;
        }
        //update display clock hours
        uint8_t leftHour = clockTime.hour / 10;
        uint8_t rightHour = clockTime.hour % 10;
              
        drawNumber(clock.startX_1, clock.startY_1, clock.width_1, clock.height_1, ST77XX_BLUE, ST77XX_BLACK, leftHour);
        drawNumber(clock.startX_2, clock.startY_2, clock.width_2, clock.height_2, ST77XX_BLUE, ST77XX_BLACK, rightHour);

        //nrf_delay_ms(500);
    }
    //update display clock minutes
    uint8_t leftMin = clockTime.minute / 10;
    uint8_t rightMin = clockTime.minute % 10;

    drawNumber(clock.startX_3, clock.startY_3, clock.width_3, clock.height_3, ST77XX_BLUE, ST77XX_BLACK, leftMin);
    drawNumber(clock.startX_4, clock.startY_4, clock.width_4, clock.height_4, ST77XX_BLUE, ST77XX_BLACK, rightMin);
    
    //nrf_delay_ms(500);
 
    SEGGER_RTT_printf(0, "Clock: %d:%d\n", clockTime.hour, clockTime.minute);
}



/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void)
{
    // Initialize timer module.
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Create timers.

    /* YOUR_JOB: Create any timers to be used by the application.
                 Below is an example of how to create a timer.
                 For every new timer needed, increase the value of the macro APP_TIMER_MAX_TIMERS by
                 one.
       ret_code_t err_code;
       err_code = app_timer_create(&m_app_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
       APP_ERROR_CHECK(err_code); */

    err_code = app_timer_create(&m_our_char_timer_id, APP_TIMER_MODE_REPEATED, timer_timeout_handler);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_our_clock_timer_id, APP_TIMER_MODE_REPEATED, clock_timer_handler);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    ret_code_t err_code;

    switch (p_evt->evt_id)
    {
        case PM_EVT_BONDED_PEER_CONNECTED:
        {
            NRF_LOG_INFO("Connected to a previously bonded device.");
        } break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
        {
            NRF_LOG_INFO("Connection secured: role: %d, conn_handle: 0x%x, procedure: %d.",
                         ble_conn_state_role(p_evt->conn_handle),
                         p_evt->conn_handle,
                         p_evt->params.conn_sec_succeeded.procedure);
        } break;

        case PM_EVT_CONN_SEC_FAILED:
        {
            /* Often, when securing fails, it shouldn't be restarted, for security reasons.
             * Other times, it can be restarted directly.
             * Sometimes it can be restarted, but only after changing some Security Parameters.
             * Sometimes, it cannot be restarted until the link is disconnected and reconnected.
             * Sometimes it is impossible, to secure the link, or the peer device does not support it.
             * How to handle this error is highly application dependent. */
        } break;

        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            // Reject pairing request from an already bonded peer.
            pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
            pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
        } break;

        case PM_EVT_STORAGE_FULL:
        {
            // Run garbage collection on the flash.
            err_code = fds_gc();
            if (err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
            {
                // Retry.
            }
            else
            {
                APP_ERROR_CHECK(err_code);
            }
        } break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
        {
            advertising_start(false);
        } break;

        case PM_EVT_PEER_DATA_UPDATE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
        } break;

        case PM_EVT_PEER_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
        } break;

        case PM_EVT_PEERS_DELETE_FAILED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
        } break;

        case PM_EVT_ERROR_UNEXPECTED:
        {
            // Assert.
            APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
        } break;

        case PM_EVT_CONN_SEC_START:
        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
        case PM_EVT_PEER_DELETE_SUCCEEDED:
        case PM_EVT_LOCAL_DB_CACHE_APPLIED:
        case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
            // This can happen when the local DB has changed.
        case PM_EVT_SERVICE_CHANGED_IND_SENT:
        case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
        default:
            break;
    }
}


/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the YYY Service events.
 * YOUR_JOB implement a service handler function depending on the event the service you are using can generate
 *
 * @details This function will be called for all YY Service events which are passed to
 *          the application.
 *
 * @param[in]   p_yy_service   YY Service structure.
 * @param[in]   p_evt          Event received from the YY Service.
 *
 *
static void on_yys_evt(ble_yy_service_t     * p_yy_service,
                       ble_yy_service_evt_t * p_evt)
{
    switch (p_evt->evt_type)
    {
        case BLE_YY_NAME_EVT_WRITE:
            APPL_LOG("[APPL]: charact written with value %s. ", p_evt->params.char_xx.value.p_str);
            break;

        default:
            // No implementation needed.
            break;
    }
}
*/

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    ret_code_t         err_code;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);
	
	// STEP 2: Add code to initialize the services used by the application.
    our_service_init (&m_our_service);
	
}


/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for starting timers.
 */
static void application_timers_start(void)
{
    /* YOUR_JOB: Start your timers. below is an example of how to start a timer.
       ret_code_t err_code;
       err_code = app_timer_start(m_app_timer_id, TIMER_INTERVAL, NULL);
       APP_ERROR_CHECK(err_code); */
    
    ret_code_t err_code; 
    err_code = app_timer_start(m_our_char_timer_id, OUR_CHAR_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_our_clock_timer_id, OUR_CLOCK_TIMER_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    SEGGER_RTT_printf(0, "SLEEP MODE ENTER!");
    ret_code_t err_code;

    err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    ret_code_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising.");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_ADV_EVT_IDLE:
            //sleep_mode_enter();
            break;

        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ret_code_t err_code = NRF_SUCCESS;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected.");
            // LED indication will be changed when advertising starts.
            break;

        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected.");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            NRF_LOG_DEBUG("GATT Client Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

    //our handler for ble event
    NRF_SDH_BLE_OBSERVER(m_our_service_observer, APP_BLE_OBSERVER_PRIO, ble_our_service_on_ble_evt, (void*) &m_our_service);
}


/**@brief Function for the Peer Manager initialization.
 */
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated when button is pressed.
 */
static void bsp_event_handler(bsp_event_t event)
{
    ret_code_t err_code;

    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break; // BSP_EVENT_SLEEP

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break; // BSP_EVENT_DISCONNECT

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break; // BSP_EVENT_KEY_0

        default:
            break;
    }
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    ret_code_t             err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
	
	// STEP 6: Declare and instantiate the scan response
	init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    ret_code_t err_code;
    bsp_event_t startup_event;

    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
/*
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}
*/

/**@brief Function for starting advertising.
 */
static void advertising_start(bool erase_bonds)
{
    if (erase_bonds == true)
    {
        delete_bonds();
        // Advertising is started by PM_EVT_PEERS_DELETED_SUCEEDED event
    }
    else
    {
        ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);

        APP_ERROR_CHECK(err_code);
    }
}
/*
void spim_event_handler(nrfx_spim_evt_t const * p_event,
                       void *                  p_context)
{
    spi_xfer_done = true;
    NRF_LOG_INFO("Transfer completed.");
    if (m_rx_buf[0] != 0)
    {
        NRF_LOG_INFO(" Received:");
        NRF_LOG_HEXDUMP_INFO(m_rx_buf, strlen((const char *)m_rx_buf));
    }
}
*/

static void spim_init(void)
{
    //xfer_desc = NRFX_SPIM_XFER_TRX(m_tx_buf, m_length, m_rx_buf, m_length);
/*
    nrf_gpio_pin_set(NRFX_SPIM_SCK_PIN);
    nrf_gpio_cfg_output(NRFX_SPIM_SCK_PIN);
    nrf_gpio_pin_clear(NRFX_SPIM_MOSI_PIN);
    nrf_gpio_cfg_output(NRFX_SPIM_MOSI_PIN);
    nrf_gpio_cfg_input(NRFX_SPIM_MISO_PIN, NRF_GPIO_PIN_NOPULL);
*/

    nrf_gpio_cfg_output(NRFX_SPIM_DCX_PIN);
    nrf_gpio_cfg_output(NRFX_SPIM_RESET_PIN);


    spi_config.frequency      = NRF_SPIM_FREQ_8M;
    spi_config.mode           = NRF_SPIM_MODE_3;
    spi_config.bit_order      = NRF_SPIM_BIT_ORDER_MSB_FIRST;
    spi_config.ss_pin         = NRFX_SPIM_SS_PIN;
    spi_config.miso_pin       = NRFX_SPIM_MISO_PIN;
    spi_config.mosi_pin       = NRFX_SPIM_MOSI_PIN;
    spi_config.sck_pin        = NRFX_SPIM_SCK_PIN;
    spi_config.ss_active_high = false;

    ret_code_t err_code = nrfx_spim_init(&lcdSpi, &spi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("NRFX SPIM example started.");
}
/*
static inline void spim_write(const void * data, size_t size)
{
    ret_code_t err_code =nrfx_spim_xfer(&spi, data, size, NULL);

    APP_ERROR_CHECK(err_code);
}
static void write_command(uint8_t c)
{
    nrf_gpio_pin_clear(NRFX_SPIM_DCX_PIN);
    spi_write(&c, sizeof(c));
}

static void write_data(uint8_t c)
{
    nrf_gpio_pin_set(NRFX_SPIM_DCX_PIN);
    spi_write(&c, sizeof(c));
}
*/
/**@brief Function for application main entry.
 */
void setCommandPin()
{
    nrf_gpio_pin_clear(NRFX_SPIM_DCX_PIN);
}

void setDataPin()
{
    nrf_gpio_pin_set(NRFX_SPIM_DCX_PIN);
}
void setWindow(uint8_t startX, uint8_t startY, uint8_t width, uint8_t height)
{

    uint8_t lcdTxCasetCmd = ST7789_CMD_CASET;
    nrfx_spim_xfer_desc_t lcdXferCasetCmd = NRFX_SPIM_XFER_TX(&lcdTxCasetCmd, 1);

    uint8_t lcdCasetData[] = {
            (startX >> 8),
            (startX & 0xFF),
            (width >> 8),
            (width & 0xFF)
    };

    nrfx_spim_xfer_desc_t lcdXferCasetData = NRFX_SPIM_XFER_TX(lcdCasetData, sizeof(lcdCasetData));

    uint8_t lcdTxRasetCmd = ST7789_CMD_RASET;
    nrfx_spim_xfer_desc_t lcdXferRasetCmd = NRFX_SPIM_XFER_TX(&lcdTxRasetCmd, 1);    

    uint8_t lcdRasetData[] = {
            (startY >> 8),
            (startY & 0xFF),
            (height >> 8),
            (height & 0xFF)
    };

    nrfx_spim_xfer_desc_t lcdXferRasetData = NRFX_SPIM_XFER_TX(lcdRasetData, sizeof(lcdRasetData));

    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferCasetCmd, 0));
    setDataPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferCasetData, 0));

    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferRasetCmd, 0));
    setDataPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferRasetData, 0));
}
void drawPixel(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color)
{

}
void drawBuffer()
{
    
}
void drawNumber(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t textColor, uint16_t bgColor, uint8_t number){
    
    setWindow(x, y, width, height);

    uint8_t bgColorData[] = {
        (uint8_t)(bgColor >> 8),
        (uint8_t)(bgColor & 0xFF)
    };
    nrfx_spim_xfer_desc_t bgData  = NRFX_SPIM_XFER_TX(&bgColorData, sizeof(bgColorData));

    uint8_t textColorData[] = {
        (uint8_t)(textColor >> 8),
        (uint8_t)(textColor & 0xFF)
    };
    nrfx_spim_xfer_desc_t textData  = NRFX_SPIM_XFER_TX(&textColorData, sizeof(textColorData));

    const uint16_t * numArray = sans_serif_30x60_get_number(number);
    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &xfer_lcd_cmd_ramwr, 0));
    setDataPin();

    for (int i = 0; i < 1800; i++)
    {
        if(numArray[i] == ST77XX_RED)
        {

            APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &bgData, 0));
        }
        else
        {
            APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &textData, 0));
        }   
       

    }
}

int main(void)
{
    bool erase_bonds;

    // Initialize.
    log_init();
	timers_init();
    buttons_leds_init(&erase_bonds);
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    peer_manager_init();
   

    // Start execution.
    NRF_LOG_INFO("started.");
    application_timers_start();

    advertising_start(erase_bonds);

    spim_init();
    // Enter main loop.

    
    const int16_t screenSizeX = 240;
    const int16_t screenSizeY = 240;


    // Hardware-reset the Screen controller
    nrf_gpio_pin_clear(NRFX_SPIM_RESET_PIN);
    nrf_delay_ms(15);

    nrf_gpio_pin_set(NRFX_SPIM_RESET_PIN);
    nrf_delay_ms(2);

    // Prepare the commands to software-reset the controller
    static uint8_t lcdTxSwreset = ST7789_CMD_SWRESET;
    static uint8_t lcdTxSlpout  = ST7789_CMD_SLPOUT;
    nrfx_spim_xfer_desc_t lcdXferSwreset = NRFX_SPIM_XFER_TX(&lcdTxSwreset, 1);
    nrfx_spim_xfer_desc_t lcdXferSlpout  = NRFX_SPIM_XFER_TX(&lcdTxSlpout, 1);

    static uint8_t lcdTxColmodCmd  = ST7789_CMD_COLMOD;
    static uint8_t lcdTxColmodData = 0x55;
    nrfx_spim_xfer_desc_t lcdXferColmodCmd  = NRFX_SPIM_XFER_TX(&lcdTxColmodCmd, 1);
    nrfx_spim_xfer_desc_t lcdXferColmodData = NRFX_SPIM_XFER_TX(&lcdTxColmodData, 1);

    static uint8_t lcdTxMadctlCmd  = ST7789_CMD_MADCTL;
    static uint8_t lcdTxMadctlData = 0;
    nrfx_spim_xfer_desc_t lcdXferMadctlCmd  = NRFX_SPIM_XFER_TX(&lcdTxMadctlCmd, 1);
    nrfx_spim_xfer_desc_t lcdXferMadctlData = NRFX_SPIM_XFER_TX(&lcdTxMadctlData, 1);

    static uint8_t lcdTxLast[] = {
            ST7789_CMD_INVON,
            ST7789_CMD_NORON,
            ST7789_CMD_DISPON
    };
    nrfx_spim_xfer_desc_t lcdXferLast = NRFX_SPIM_XFER_TX(lcdTxLast, sizeof(lcdTxLast));

    //write
    
    uint16_t bgColor = ST77XX_BLACK;
    uint8_t lcdTxRamwrData[] = {
        (uint8_t)(bgColor >> 8),
        (uint8_t)(bgColor & 0xFF) 
    };

    uint8_t lcdcmd = ST7789_CMD_RAMWR;

    
    nrfx_spim_xfer_desc_t xferCmd  = NRFX_SPIM_XFER_TX(&lcdcmd, 1);
    nrfx_spim_xfer_desc_t xferData = NRFX_SPIM_XFER_TX(lcdTxRamwrData, sizeof(lcdTxRamwrData));

 

    // Send the commands
    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferSwreset, 0));
    nrf_delay_ms(150);

    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferSlpout, 0));
    nrf_delay_ms(10);

    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferColmodCmd, 0));
    setDataPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferColmodData, 0));
    nrf_delay_ms(10);

    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferMadctlCmd, 0));
    setDataPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferMadctlData, 0));

    setWindow(0, 0, screenSizeX, screenSizeY);

    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &lcdXferLast, 0));
    nrf_delay_ms(10);

    //write bg black
    
    setCommandPin();
    APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &xferCmd, 0));
    setDataPin();

    int sbs = 57600;

    for(int i = 0; i < sbs; i++){
        APP_ERROR_CHECK(nrfx_spim_xfer(&lcdSpi, &xferData, 0));
    }
    
    //clock
    drawNumber(clock.startX_1, clock.startY_1, clock.width_1, clock.height_1, ST77XX_BLUE, ST77XX_BLACK, clockTime.hour / 10);
    drawNumber(clock.startX_2, clock.startY_2, clock.width_2, clock.height_2, ST77XX_BLUE, ST77XX_BLACK, clockTime.hour % 10);
    drawNumber(clock.startX_3, clock.startY_3, clock.width_3, clock.height_3, ST77XX_BLUE, ST77XX_BLACK, clockTime.minute / 10);
    drawNumber(clock.startX_4, clock.startY_4, clock.width_4, clock.height_4, ST77XX_BLUE, ST77XX_BLACK, clockTime.minute % 10);
    

    for (;;)
    {
        
        //NRF_LOG_INFO("test ending");
        //NRF_LOG_FLUSH();
    }
        while (!spi_xfer_done)
        {
            __WFE();
        }

        NRF_LOG_FLUSH();

        bsp_board_led_invert(BSP_BOARD_LED_0);
        nrf_delay_ms(200);
        
}


/**
 * @}
 */
