/******************************************************************************

 @file  simpleBLECentral.c

 @brief This file contains the Simple BLE Central sample application for use
        with the CC2540 Bluetooth Low Energy Protocol Stack.

 Group: WCS, BTS
 Target Device: CC2540, CC2541

 ******************************************************************************
 
 Copyright (c) 2010-2016, Texas Instruments Incorporated
 All rights reserved.

 IMPORTANT: Your use of this Software is limited to those specific rights
 granted under the terms of a software license agreement between the user
 who downloaded the software, his/her employer (which must be your employer)
 and Texas Instruments Incorporated (the "License"). You may not use this
 Software unless you agree to abide by the terms of the License. The License
 limits your use, and you acknowledge, that the Software may not be modified,
 copied or distributed unless embedded on a Texas Instruments microcontroller
 or used solely and exclusively in conjunction with a Texas Instruments radio
 frequency transceiver, which is integrated into your product. Other than for
 the foregoing purpose, you may not use, reproduce, copy, prepare derivative
 works of, modify, distribute, perform, display or sell this Software and/or
 its documentation for any purpose.

 YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
 PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
 NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
 TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
 NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
 LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
 INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
 OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
 OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
 (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

 Should you have any questions regarding your right to use this Software,
 contact Texas Instruments Incorporated at www.TI.com.

 ******************************************************************************
 Release Name: ble_sdk_1.4.2.2
 Release Date: 2016-06-09 06:57:10
 *****************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"
#include "OnBoard.h"
#include "hal_led.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "gatt.h"
#include "ll.h"
#include "hci.h"
#include "gapgattserver.h"
#include "gattservapp.h"
#include "central.h"
#include "gapbondmgr.h"
#include "simpleGATTprofile.h"
#include "simpleBLECentral.h"
#include "npi.h"
/*********************************************************************
 * MACROS
 */

// Length of bd addr as a string
#define B_ADDR_STR_LEN                        15

/*********************************************************************
 * CONSTANTS
 */

// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES                  8

// Scan duration in ms
#define DEFAULT_SCAN_DURATION                 4000

// Discovey mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE                DEVDISC_MODE_ALL

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN         TRUE

// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST          FALSE

// TRUE to use high scan duty cycle when creating link
#define DEFAULT_LINK_HIGH_DUTY_CYCLE          FALSE

// TRUE to use white list when creating link
#define DEFAULT_LINK_WHITE_LIST               FALSE

// Default RSSI polling period in ms
#define DEFAULT_RSSI_PERIOD                   1000

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         FALSE

// Minimum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MIN_CONN_INTERVAL      400

// Maximum connection interval (units of 1.25ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_MAX_CONN_INTERVAL      800

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_UPDATE_SLAVE_LATENCY          0

// Supervision timeout value (units of 10ms) if automatic parameter update request is enabled
#define DEFAULT_UPDATE_CONN_TIMEOUT           600

// Default passcode
#define DEFAULT_PASSCODE                      19655

// Default GAP pairing mode
#define DEFAULT_PAIRING_MODE                  GAPBOND_PAIRING_MODE_WAIT_FOR_REQ

// Default MITM mode (TRUE to require passcode or OOB when pairing)
#define DEFAULT_MITM_MODE                     FALSE

// Default bonding mode, TRUE to bond
#define DEFAULT_BONDING_MODE                  TRUE

// Default GAP bonding I/O capabilities
#define DEFAULT_IO_CAPABILITIES               GAPBOND_IO_CAP_DISPLAY_ONLY

// Default service discovery timer delay in ms
#define DEFAULT_SVC_DISCOVERY_DELAY           1000

// TRUE to filter discovery results on desired service UUID
#define DEFAULT_DEV_DISC_BY_SVC_UUID          TRUE

// Application states
enum
{
  BLE_STATE_IDLE,
  BLE_STATE_CONNECTING,
  BLE_STATE_CONNECTED,
  BLE_STATE_DISCONNECTING
};

// Discovery states
enum
{
  BLE_DISC_STATE_IDLE,                // Idle
  BLE_DISC_STATE_SVC,                 // Service discovery
  BLE_DISC_STATE_CHAR                 // Characteristic discovery
};

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

// Task ID for internal task/event processing
static uint8 simpleBLETaskId;

// GAP GATT Attributes
static const uint8 simpleBLEDeviceName[GAP_DEVICE_NAME_LEN] = "Simple BLE Central";

// Number of scan results and scan result index
static uint8 simpleBLEScanRes;
static uint8 simpleBLEScanIdx;

// Scan result list
static gapDevRec_t simpleBLEDevList[DEFAULT_MAX_SCAN_RES];

// Scanning state
static uint8 simpleBLEScanning = FALSE;

// RSSI polling state
static uint8 simpleBLERssi = FALSE;

// Connection handle of current connection 
static uint16 simpleBLEConnHandle = GAP_CONNHANDLE_INIT;

// Application state
static uint8 simpleBLEState = BLE_STATE_IDLE;

// Discovery state
static uint8 simpleBLEDiscState = BLE_DISC_STATE_IDLE;

// Discovered service start and end handle
static uint16 simpleBLESvcStartHdl = 0;
static uint16 simpleBLESvcEndHdl = 0;

// Discovered characteristic handle
static uint16 simpleBLECharHdl = 0;

// Value to write
static uint8 simpleBLECharVal = 0;

// Value read/write toggle
static bool simpleBLEDoWrite = FALSE;

// GATT read/write procedure state
static bool simpleBLEProcedureInProgress = FALSE;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void simpleBLECentralProcessGATTMsg( gattMsgEvent_t *pMsg );
static void simpleBLECentralRssiCB( uint16 connHandle, int8  rssi );
static uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent );
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs );
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status );
static void simpleBLECentral_HandleKeys( uint8 shift, uint8 keys );
static void simpleBLECentral_ProcessOSALMsg( osal_event_hdr_t *pMsg );
static void simpleBLEGATTDiscoveryEvent( gattMsgEvent_t *pMsg );
static void simpleBLECentralStartDiscovery( void );
static bool simpleBLEFindSvcUuid( uint16 uuid, uint8 *pData, uint8 dataLen );
static void simpleBLEAddDeviceInfo( uint8 *pAddr, uint8 addrType );
char *bdAddr2Str ( uint8 *pAddr );
static void NpiSerialCallback( uint8 port, uint8 events );
/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static const gapCentralRoleCB_t simpleBLERoleCB =
{
  simpleBLECentralRssiCB,       // RSSI callback
  simpleBLECentralEventCB       // Event callback
};

// Bond Manager Callbacks
static const gapBondCBs_t simpleBLEBondCB =
{
  simpleBLECentralPasscodeCB,
  simpleBLECentralPairStateCB
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      SimpleBLECentral_Init
 *
 * @brief   Initialization function for the Simple BLE Central App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void SimpleBLECentral_Init( uint8 task_id )
{
  simpleBLETaskId = task_id;

  // Setup Central Profile
  {
    uint8 scanRes = DEFAULT_MAX_SCAN_RES;
    GAPCentralRole_SetParameter ( GAPCENTRALROLE_MAX_SCAN_RES, sizeof( uint8 ), &scanRes );
  }
  
  // Setup GAP
  GAP_SetParamValue( TGAP_GEN_DISC_SCAN, DEFAULT_SCAN_DURATION );
  GAP_SetParamValue( TGAP_LIM_DISC_SCAN, DEFAULT_SCAN_DURATION );
  GGS_SetParameter( GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, (uint8 *) simpleBLEDeviceName );

  // Setup the GAP Bond Manager
  {
    uint32 passkey = DEFAULT_PASSCODE;
    uint8 pairMode = DEFAULT_PAIRING_MODE;
    uint8 mitm = DEFAULT_MITM_MODE;
    uint8 ioCap = DEFAULT_IO_CAPABILITIES;
    uint8 bonding = DEFAULT_BONDING_MODE;
    GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof( uint32 ), &passkey );
    GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof( uint8 ), &pairMode );
    GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof( uint8 ), &mitm );
    GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof( uint8 ), &ioCap );
    GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof( uint8 ), &bonding );
  }  

  // Initialize GATT Client
  VOID GATT_InitClient();

  // Register to receive incoming ATT Indications/Notifications
  GATT_RegisterForInd( simpleBLETaskId );

  // Initialize GATT attributes
  GGS_AddService( GATT_ALL_SERVICES );         // GAP
  GATTServApp_AddService( GATT_ALL_SERVICES ); // GATT attributes

  // Register for all key events - This app will handle all key events
  RegisterForKeys( simpleBLETaskId );
  
  // makes sure LEDs are off
  HalLedSet( (HAL_LED_1 | HAL_LED_2), HAL_LED_MODE_OFF );
  
  // Setup a delayed profile startup
  osal_set_event( simpleBLETaskId, START_DEVICE_EVT );
  
  //��ʼ������  
NPI_InitTransport(NpiSerialCallback);     
    /*
// ���������2    
NPI_WriteTransport("I'm coming\r\n", 20);    
    
// ���ַ������    
NPI_PrintString("SimpleBLETest_Init2\r\n");      
    */
// �������һ��ֵ����10���Ʊ�ʾ    
NPI_PrintValue("����Ĵ����1 = ", 168, 10);    
NPI_PrintString("\r\n");      
    
// �������һ��ֵ����16���Ʊ�ʾ    
NPI_PrintValue("����Ĵ����2 = 0x", 0x88, 16);    
NPI_PrintString("\r\n");   
}

/*********************************************************************
 * @fn      SimpleBLECentral_ProcessEvent
 *
 * @brief   Simple BLE Central Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 SimpleBLECentral_ProcessEvent( uint8 task_id, uint16 events )
{
  
  VOID task_id; // OSAL required parameter that isn't used in this function
  
  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;

    if ( (pMsg = osal_msg_receive( simpleBLETaskId )) != NULL )
    {
      simpleBLECentral_ProcessOSALMsg( (osal_event_hdr_t *)pMsg );

      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if ( events & START_DEVICE_EVT )
  {
    // Start the Device
    VOID GAPCentralRole_StartDevice( (gapCentralRoleCB_t *) &simpleBLERoleCB );

    // Register with bond manager after starting device
    GAPBondMgr_Register( (gapBondCBs_t *) &simpleBLEBondCB );

    return ( events ^ START_DEVICE_EVT );
  }

  if ( events & START_DISCOVERY_EVT )
  {
    simpleBLECentralStartDiscovery( );
    
    return ( events ^ START_DISCOVERY_EVT );
  }
  
  // Discard unknown events
  return 0;
}

/*********************************************************************
 * @fn      simpleBLECentral_ProcessOSALMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void simpleBLECentral_ProcessOSALMsg( osal_event_hdr_t *pMsg )
{
  switch ( pMsg->event )
  {
    case KEY_CHANGE:
      simpleBLECentral_HandleKeys( ((keyChange_t *)pMsg)->state, ((keyChange_t *)pMsg)->keys );
      break;

    case GATT_MSG_EVENT:
      simpleBLECentralProcessGATTMsg( (gattMsgEvent_t *) pMsg );
      break;
  }
}

/*********************************************************************
 * @fn      simpleBLECentral_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
uint8 gStatus;
static void simpleBLECentral_HandleKeys( uint8 shift, uint8 keys )
{
  (void)shift;  // Intentionally unreferenced parameter

  if ( keys & HAL_KEY_UP )
  {
    // Start or stop discovery
    if ( simpleBLEState != BLE_STATE_CONNECTED )
    {
      if ( !simpleBLEScanning )
      {
        simpleBLEScanning = TRUE;
        simpleBLEScanRes = 0;
        
        LCD_WRITE_STRING( "Discovering...", HAL_LCD_LINE_1 );
        LCD_WRITE_STRING( "", HAL_LCD_LINE_2 );
        
        GAPCentralRole_StartDiscovery( DEFAULT_DISCOVERY_MODE,
                                       DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                       DEFAULT_DISCOVERY_WHITE_LIST );      
      }
      else
      {
        GAPCentralRole_CancelDiscovery();
      }
    }
    else if ( simpleBLEState == BLE_STATE_CONNECTED &&
              simpleBLECharHdl != 0 &&
              simpleBLEProcedureInProgress == FALSE )
    {
      uint8 status;
      
      // Do a read or write as long as no other read or write is in progress
      if ( simpleBLEDoWrite )
      {
        // Do a write
        attWriteReq_t req;
        
        req.pValue = GATT_bm_alloc( simpleBLEConnHandle, ATT_WRITE_REQ, 1, NULL );
        if ( req.pValue != NULL )
        {
          req.handle = simpleBLECharHdl;
          req.len = 1;
          req.pValue[0] = simpleBLECharVal;
          req.sig = 0;
          req.cmd = 0;
          status = GATT_WriteCharValue( simpleBLEConnHandle, &req, simpleBLETaskId );
          if ( status != SUCCESS )
          {
            GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
          }
        }
        else
        {
          status = bleMemAllocError;
        }
      }
      else
      {
        // Do a read
        attReadReq_t req;
        
        req.handle = simpleBLECharHdl;
        status = GATT_ReadCharValue( simpleBLEConnHandle, &req, simpleBLETaskId );
      }
      
      if ( status == SUCCESS )
      {
        simpleBLEProcedureInProgress = TRUE;
        simpleBLEDoWrite = !simpleBLEDoWrite;
      }
    }    
  }

  if ( keys & HAL_KEY_LEFT )
  {
    // Display discovery results
    if ( !simpleBLEScanning && simpleBLEScanRes > 0 )
    {
        // Increment index of current result (with wraparound)
        simpleBLEScanIdx++;
        if ( simpleBLEScanIdx >= simpleBLEScanRes )
        {
          simpleBLEScanIdx = 0;
        }
        //
        //NPI_PrintValue("device \r\n", simpleBLEScanIdx + 1, 10);
        NPI_PrintString("bdAddr2Str( simpleBLEDevList[simpleBLEScanIdx].addr\r\n");
       /* LCD_WRITE_STRING_VALUE( "Device", simpleBLEScanIdx + 1,
                                10, HAL_LCD_LINE_1 );
        LCD_WRITE_STRING( bdAddr2Str( simpleBLEDevList[simpleBLEScanIdx].addr ),
                          HAL_LCD_LINE_2 );*/
    }
  }

  if ( keys & HAL_KEY_RIGHT )
  {
    // Connection update
    if ( simpleBLEState == BLE_STATE_CONNECTED )
    {
      GAPCentralRole_UpdateLink( simpleBLEConnHandle,
                                 DEFAULT_UPDATE_MIN_CONN_INTERVAL,
                                 DEFAULT_UPDATE_MAX_CONN_INTERVAL,
                                 DEFAULT_UPDATE_SLAVE_LATENCY,
                                 DEFAULT_UPDATE_CONN_TIMEOUT );
    }
  }
  
  if ( keys & HAL_KEY_CENTER )
  {
    uint8 addrType;
    uint8 *peerAddr;
    
    // Connect or disconnect
    if ( simpleBLEState == BLE_STATE_IDLE )
    {
      // if there is a scan result
      if ( simpleBLEScanRes > 0 )
      {
        // connect to current device in scan result
        peerAddr = simpleBLEDevList[simpleBLEScanIdx].addr;
        addrType = simpleBLEDevList[simpleBLEScanIdx].addrType;
      
        simpleBLEState = BLE_STATE_CONNECTING;
        
        GAPCentralRole_EstablishLink( DEFAULT_LINK_HIGH_DUTY_CYCLE,
                                      DEFAULT_LINK_WHITE_LIST,
                                      addrType, peerAddr );
  
        LCD_WRITE_STRING( "Connecting", HAL_LCD_LINE_1 );
        LCD_WRITE_STRING( bdAddr2Str( peerAddr ), HAL_LCD_LINE_2 ); 
      }
    }
    else if ( simpleBLEState == BLE_STATE_CONNECTING ||
              simpleBLEState == BLE_STATE_CONNECTED )
    {
      // disconnect
      simpleBLEState = BLE_STATE_DISCONNECTING;

      gStatus = GAPCentralRole_TerminateLink( simpleBLEConnHandle );
      
      LCD_WRITE_STRING( "Disconnecting", HAL_LCD_LINE_1 ); 
    }
  }
  
  if ( keys & HAL_KEY_DOWN )
  {
    // Start or cancel RSSI polling
    if ( simpleBLEState == BLE_STATE_CONNECTED )
    {
      if ( !simpleBLERssi )
      {
        simpleBLERssi = TRUE;
        GAPCentralRole_StartRssi( simpleBLEConnHandle, DEFAULT_RSSI_PERIOD );
      }
      else
      {
        simpleBLERssi = FALSE;
        GAPCentralRole_CancelRssi( simpleBLEConnHandle );
        
        LCD_WRITE_STRING( "RSSI Cancelled", HAL_LCD_LINE_1 );
      }
    }
  }
}

/*********************************************************************
 * @fn      simpleBLECentralProcessGATTMsg
 *
 * @brief   Process GATT messages
 *
 * @return  none
 */
static void simpleBLECentralProcessGATTMsg( gattMsgEvent_t *pMsg )
{
  if ( simpleBLEState != BLE_STATE_CONNECTED )
  {
    // In case a GATT message came after a connection has dropped,
    // ignore the message
    return;
  }
  
  
  if ( ( pMsg->method == ATT_READ_RSP ) ||
       ( ( pMsg->method == ATT_ERROR_RSP ) &&
         ( pMsg->msg.errorRsp.reqOpcode == ATT_READ_REQ ) ) )
  {
    if ( pMsg->method == ATT_ERROR_RSP )
    {
      uint8 status = pMsg->msg.errorRsp.errCode;
      
      LCD_WRITE_STRING_VALUE( "Read Error", status, 10, HAL_LCD_LINE_1 );
    }
    else
    {
      // After a successful read, display the read value
      uint8 valueRead = pMsg->msg.readRsp.pValue[0];

      LCD_WRITE_STRING_VALUE( "Read rsp:", valueRead, 10, HAL_LCD_LINE_1 );
    }
    
    simpleBLEProcedureInProgress = FALSE;
  }
  else if( ( pMsg->method == ATT_HANDLE_VALUE_NOTI)||
           ( pMsg->method == ATT_ERROR_RSP ) ||
           (pMsg->method == ATT_HANDLE_VALUE_IND))
  {
      volatile attHandleValueNoti_t noti;

    if ( pMsg->method == ATT_ERROR_RSP )
    {
      uint8 status = pMsg->msg.errorRsp.errCode;

    }
    else
    {
      static uint8 notifyData[20];
      // After a successful read, display the read value
      //uint8 valueRead = pMsg->msg.readRsp.value[0];
      noti.handle = pMsg->msg.handleValueNoti.handle;
      noti.len = pMsg->msg.handleValueNoti.len;
      osal_memcpy(notifyData, pMsg->msg.handleValueNoti.pValue,noti.len);

      HalUARTWrite( NPI_UART_PORT, notifyData, noti.len );
 //NPI_PrintString("notify\r\n");
    }
  }
  else if ( ( pMsg->method == ATT_WRITE_RSP ) ||
       ( ( pMsg->method == ATT_ERROR_RSP ) &&
         ( pMsg->msg.errorRsp.reqOpcode == ATT_WRITE_REQ ) ) )
  {
    
    if ( pMsg->method == ATT_ERROR_RSP == ATT_ERROR_RSP )
    {
      uint8 status = pMsg->msg.errorRsp.errCode;
      
      LCD_WRITE_STRING_VALUE( "Write Error", status, 10, HAL_LCD_LINE_1 );
    }
    else
    {
      // After a succesful write, display the value that was written and increment value
      LCD_WRITE_STRING_VALUE( "Write sent:", simpleBLECharVal++, 10, HAL_LCD_LINE_1 );      
    }
    
    simpleBLEProcedureInProgress = FALSE;    

  }
  else if ( simpleBLEDiscState != BLE_DISC_STATE_IDLE )
  {
    simpleBLEGATTDiscoveryEvent( pMsg );
  }
  
  GATT_bm_free( &pMsg->msg, pMsg->method );
}

/*********************************************************************
 * @fn      simpleBLECentralRssiCB
 *
 * @brief   RSSI callback.
 *
 * @param   connHandle - connection handle
 * @param   rssi - RSSI
 *
 * @return  none
 */
static void simpleBLECentralRssiCB( uint16 connHandle, int8 rssi )
{
    LCD_WRITE_STRING_VALUE( "RSSI -dB:", (uint8) (-rssi), 10, HAL_LCD_LINE_1 );
}

/*********************************************************************
 * @fn      simpleBLECentralEventCB
 *
 * @brief   Central event callback function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  TRUE if safe to deallocate event message, FALSE otherwise.
 */
static uint8 simpleBLECentralEventCB( gapCentralRoleEvent_t *pEvent )
{
  switch ( pEvent->gap.opcode )
  {
    case GAP_DEVICE_INIT_DONE_EVENT:  
      {
        LCD_WRITE_STRING( "BLE Central", HAL_LCD_LINE_1 );
        LCD_WRITE_STRING( bdAddr2Str( pEvent->initDone.devAddr ),  HAL_LCD_LINE_2 );
      }
      break;

    case GAP_DEVICE_INFO_EVENT:
      {
        // if filtering device discovery results based on service UUID
        if ( DEFAULT_DEV_DISC_BY_SVC_UUID == TRUE )
        {
          if ( simpleBLEFindSvcUuid( SIMPLEPROFILE_SERV_UUID,
                                     pEvent->deviceInfo.pEvtData,
                                     pEvent->deviceInfo.dataLen ) )
          {
            simpleBLEAddDeviceInfo( pEvent->deviceInfo.addr, pEvent->deviceInfo.addrType );
          }
        }
      }
      break;
      
    case GAP_DEVICE_DISCOVERY_EVENT:
      {
        // discovery complete
        simpleBLEScanning = FALSE;

        // if not filtering device discovery results based on service UUID
   //    if ( DEFAULT_DEV_DISC_BY_SVC_UUID == FALSE )
        {
          // Copy results
          simpleBLEScanRes = pEvent->discCmpl.numDevs;
          osal_memcpy( simpleBLEDevList, pEvent->discCmpl.pDevList,
                       (sizeof( gapDevRec_t ) * pEvent->discCmpl.numDevs) );
        }
        
        LCD_WRITE_STRING_VALUE( "Devices Found", simpleBLEScanRes,
                                10, HAL_LCD_LINE_1 );
          for(unsigned char i = 0;i<simpleBLEScanRes;i++)
          {
            NPI_PrintString("Devices ");
          //  NPI_PrintValue("0x ",*simpleBLEDevList[i].addr,10);
            NPI_PrintString(bdAddr2Str(simpleBLEDevList[i].addr));
             NPI_PrintString("\r\n");
          }
        GAPCentralRole_CancelDiscovery();
      //   NPI_PrintString("Devices Found\r\n");
        if ( simpleBLEScanRes > 0 )
        {
          LCD_WRITE_STRING( "<- To Select", HAL_LCD_LINE_2 );
        }

        // initialize scan index to last device
        simpleBLEScanIdx = simpleBLEScanRes;

      }
      break;

    case GAP_LINK_ESTABLISHED_EVENT:
      {
        if ( pEvent->gap.hdr.status == SUCCESS )
        {          
          simpleBLEState = BLE_STATE_CONNECTED;
          simpleBLEConnHandle = pEvent->linkCmpl.connectionHandle;
          simpleBLEProcedureInProgress = TRUE;    

          // If service discovery not performed initiate service discovery
          if ( simpleBLECharHdl == 0 )
          {
            osal_start_timerEx( simpleBLETaskId, START_DISCOVERY_EVT, DEFAULT_SVC_DISCOVERY_DELAY );
          }
           NPI_PrintString("connected device  ");     
          NPI_PrintString(bdAddr2Str( pEvent->linkCmpl.devAddr));
          NPI_PrintString("\r\n");
          LCD_WRITE_STRING( "Connected", HAL_LCD_LINE_1 );
          LCD_WRITE_STRING( bdAddr2Str( pEvent->linkCmpl.devAddr ), HAL_LCD_LINE_2 );   
        }
        else
        {
          simpleBLEState = BLE_STATE_IDLE;
          simpleBLEConnHandle = GAP_CONNHANDLE_INIT;
          simpleBLERssi = FALSE;
          simpleBLEDiscState = BLE_DISC_STATE_IDLE;
          
          LCD_WRITE_STRING( "Connect Failed", HAL_LCD_LINE_1 );
          LCD_WRITE_STRING_VALUE( "Reason:", pEvent->gap.hdr.status, 10, HAL_LCD_LINE_2 );
        }
      }
      break;

    case GAP_LINK_TERMINATED_EVENT:
      {
        simpleBLEState = BLE_STATE_IDLE;
        simpleBLEConnHandle = GAP_CONNHANDLE_INIT;
        simpleBLERssi = FALSE;
        simpleBLEDiscState = BLE_DISC_STATE_IDLE;
        simpleBLECharHdl = 0;
        simpleBLEProcedureInProgress = FALSE;
        NPI_PrintString("disconnected\r\n");  
        LCD_WRITE_STRING( "Disconnected", HAL_LCD_LINE_1 );
        LCD_WRITE_STRING_VALUE( "Reason:", pEvent->linkTerminate.reason,
                                10, HAL_LCD_LINE_2 );
      }
      break;

    case GAP_LINK_PARAM_UPDATE_EVENT:
      {
        LCD_WRITE_STRING( "Param Update", HAL_LCD_LINE_1 );
      }
      break;
      
    default:
      break;
  }
  
  return ( TRUE );
}

/*********************************************************************
 * @fn      pairStateCB
 *
 * @brief   Pairing state callback.
 *
 * @return  none
 */
static void simpleBLECentralPairStateCB( uint16 connHandle, uint8 state, uint8 status )
{
  if ( state == GAPBOND_PAIRING_STATE_STARTED )
  {
    LCD_WRITE_STRING( "Pairing started", HAL_LCD_LINE_1 );
  }
  else if ( state == GAPBOND_PAIRING_STATE_COMPLETE )
  {
    if ( status == SUCCESS )
    {
      LCD_WRITE_STRING( "Pairing success", HAL_LCD_LINE_1 );
    }
    else
    {
      LCD_WRITE_STRING_VALUE( "Pairing fail", status, 10, HAL_LCD_LINE_1 );
    }
  }
  else if ( state == GAPBOND_PAIRING_STATE_BONDED )
  {
    if ( status == SUCCESS )
    {
      LCD_WRITE_STRING( "Bonding success", HAL_LCD_LINE_1 );
    }
  }
}

/*********************************************************************
 * @fn      simpleBLECentralPasscodeCB
 *
 * @brief   Passcode callback.
 *
 * @return  none
 */
static void simpleBLECentralPasscodeCB( uint8 *deviceAddr, uint16 connectionHandle,
                                        uint8 uiInputs, uint8 uiOutputs )
{
#if (HAL_LCD == TRUE)

  uint32  passcode;
  uint8   str[7];

  // Create random passcode
  LL_Rand( ((uint8 *) &passcode), sizeof( uint32 ));
  passcode %= 1000000;
  
  // Display passcode to user
  if ( uiOutputs != 0 )
  {
    LCD_WRITE_STRING( "Passcode:",  HAL_LCD_LINE_1 );
    LCD_WRITE_STRING( (char *) _ltoa(passcode, str, 10),  HAL_LCD_LINE_2 );
  }
  
  // Send passcode response
  GAPBondMgr_PasscodeRsp( connectionHandle, SUCCESS, passcode );
#endif
}

/*********************************************************************
 * @fn      simpleBLECentralStartDiscovery
 *
 * @brief   Start service discovery.
 *
 * @return  none
 */
static void simpleBLECentralStartDiscovery( void )
{
  /*uint8 uuid[ATT_BT_UUID_SIZE] = { LO_UINT16(SIMPLEPROFILE_SERV_UUID),
                                   HI_UINT16(SIMPLEPROFILE_SERV_UUID) };*/
 
  uint8 uuid[ATT_UUID_SIZE] = {0x8b, 0xb9, 0xb1, 0xcd, 0x90, 0x4c, 0x4d, 0xe3, 0xbb, 0x7b, 0xbb, 0x27, 0xb2, 0x2e, 0xdd, 0xe6};
  // Initialize cached handles
  simpleBLESvcStartHdl = simpleBLESvcEndHdl = simpleBLECharHdl = 0;

  simpleBLEDiscState = BLE_DISC_STATE_SVC;
 // GATT_DiscAllChars(simpleBLEConnHandle,14,20,simpleBLETaskId);
  // Discovery simple BLE service
  GATT_DiscPrimaryServiceByUUID( simpleBLEConnHandle,
                                 uuid,
                                 ATT_UUID_SIZE,
                                 simpleBLETaskId );
}

/*********************************************************************
 * @fn      simpleBLEGATTDiscoveryEvent
 *
 * @brief   Process GATT discovery event
 *
 * @return  none
 */
static void simpleBLEGATTDiscoveryEvent( gattMsgEvent_t *pMsg )
{
  attReadByTypeReq_t req;
  
  if ( simpleBLEDiscState == BLE_DISC_STATE_SVC )
  {
    // Service found, store handles
    if ( pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP &&
         pMsg->msg.findByTypeValueRsp.numInfo > 0 )
    {
      simpleBLESvcStartHdl = ATT_ATTR_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
      simpleBLESvcEndHdl = ATT_GRP_END_HANDLE(pMsg->msg.findByTypeValueRsp.pHandlesInfo, 0);
    }
    
    // If procedure complete
    if ( ( pMsg->method == ATT_FIND_BY_TYPE_VALUE_RSP  && 
           pMsg->hdr.status == bleProcedureComplete ) ||
         ( pMsg->method == ATT_ERROR_RSP ) )
    {
      if ( simpleBLESvcStartHdl != 0 )
      {
        // Discover characteristic
        simpleBLEDiscState = BLE_DISC_STATE_CHAR;
        
        req.startHandle = simpleBLESvcStartHdl;
        req.endHandle = simpleBLESvcEndHdl;
        req.type.len = ATT_UUID_SIZE;
        uint8 uuid_t[16] = {0x8b, 0xb9, 0xb1, 0xcd, 0x90, 0x4c, 0x4d, 0xe3, 0xbb, 0x7b, 0xbb, 0x27, 0xb2, 0x2e, 0xdd, 0xe6};
        memcpy(req.type.uuid,uuid_t,16);
     //   req.type.uuid ={0x8b, 0xb9, 0xb1, 0xcd, 0x90, 0x4c, 0x4d, 0xe3, 0xbb, 0x7b, 0xbb, 0x27, 0xb2, 0x2e, 0xdd, 0xe6};
       // req.type.uuid[1] = HI_UINT16(SIMPLEPROFILE_CHAR1_UUID);

        GATT_ReadUsingCharUUID( simpleBLEConnHandle, &req, simpleBLETaskId );
      }
    }
  }
  else if ( simpleBLEDiscState == BLE_DISC_STATE_CHAR )
  {
    // Characteristic found, store handle
    if ( pMsg->method == ATT_READ_BY_TYPE_RSP && 
         pMsg->msg.readByTypeRsp.numPairs > 0 )
    {
      simpleBLECharHdl = BUILD_UINT16(pMsg->msg.readByTypeRsp.pDataList[0],
                                      pMsg->msg.readByTypeRsp.pDataList[1]);
      
      LCD_WRITE_STRING( "Simple Svc Found", HAL_LCD_LINE_1 );
      simpleBLEProcedureInProgress = FALSE;
    }
    
    simpleBLEDiscState = BLE_DISC_STATE_IDLE;

    
  }    
}


/*********************************************************************
 * @fn      simpleBLEFindSvcUuid
 *
 * @brief   Find a given UUID in an advertiser's service UUID list.
 *
 * @return  TRUE if service UUID found
 */
static bool simpleBLEFindSvcUuid( uint16 uuid, uint8 *pData, uint8 dataLen )
{
  uint8 adLen;
  uint8 adType;
  uint8 *pEnd;
  
  pEnd = pData + dataLen - 1;
  
  // While end of data not reached
  while ( pData < pEnd )
  {
    // Get length of next AD item
    adLen = *pData++;
    if ( adLen > 0 )
    {
      adType = *pData;
      
      // If AD type is for 16-bit service UUID
      if ( adType == GAP_ADTYPE_16BIT_MORE || adType == GAP_ADTYPE_16BIT_COMPLETE )
      {
        pData++;
        adLen--;
        
        // For each UUID in list
        while ( adLen >= 2 && pData < pEnd )
        {
          // Check for match
          if ( pData[0] == LO_UINT16(uuid) && pData[1] == HI_UINT16(uuid) )
          {
            // Match found
            return TRUE;
          }
          
          // Go to next
          pData += 2;
          adLen -= 2;
        }
        
        // Handle possible erroneous extra byte in UUID list
        if ( adLen == 1 )
        {
          pData++;
        }
      }
      else
      {
        // Go to next item
        pData += adLen;
      }
    }
  }
  
  // Match not found
  return FALSE;
}

/*********************************************************************
 * @fn      simpleBLEAddDeviceInfo
 *
 * @brief   Add a device to the device discovery result list
 *
 * @return  none
 */
static void simpleBLEAddDeviceInfo( uint8 *pAddr, uint8 addrType )
{
  uint8 i;
  
  // If result count not at max
  if ( simpleBLEScanRes < DEFAULT_MAX_SCAN_RES )
  {
    // Check if device is already in scan results
    for ( i = 0; i < simpleBLEScanRes; i++ )
    {
      if ( osal_memcmp( pAddr, simpleBLEDevList[i].addr , B_ADDR_LEN ) )
      {
        return;
      }
    }
    
    // Add addr to scan result list
    osal_memcpy( simpleBLEDevList[simpleBLEScanRes].addr, pAddr, B_ADDR_LEN );
    simpleBLEDevList[simpleBLEScanRes].addrType = addrType;
    
    // Increment scan result count
    simpleBLEScanRes++;
  }
}

/*********************************************************************
 * @fn      bdAddr2Str
 *
 * @brief   Convert Bluetooth address to string
 *
 * @return  none
 */
char *bdAddr2Str( uint8 *pAddr )
{
  uint8       i;
  char        hex[] = "0123456789ABCDEF";
  static char str[B_ADDR_STR_LEN];
  char        *pStr = str;
  
  *pStr++ = '0';
  *pStr++ = 'x';
  
  // Start from end of addr
  pAddr += B_ADDR_LEN;
  
  for ( i = B_ADDR_LEN; i > 0; i-- )
  {
    *pStr++ = hex[*--pAddr >> 4];
    *pStr++ = hex[*pAddr & 0x0F];
  }
  
  *pStr = 0;
  
  return str;
}

static void NpiSerialCallback( uint8 port, uint8 events )  
{  
    (void)port;//�Ӹ� (void)����δ�˱������澯����ȷ���߻�������������������  
  
    if (events & (HAL_UART_RX_TIMEOUT | HAL_UART_RX_FULL))   //����������  
    {  
        uint8 numBytes = 0;  
  
        numBytes = NPI_RxBufLen();           //�������ڻ������ж����ֽ�  
          
        if(numBytes == 0)  
        {  
            return;  
        }  
        else  
        {  
            //���뻺����buffer  
            uint8 *buffer = osal_mem_alloc(numBytes);  
            if(buffer)  
            {  
                //��ȡ��ȡ���ڻ��������ݣ��ͷŴ�������     
                NPI_ReadTransport(buffer,numBytes);     
                if(buffer[0] == 0x01)
                {
                  NPI_PrintString("scanning...\r\n");
                  GAPCentralRole_StartDiscovery( DEFAULT_DISCOVERY_MODE,
                                       DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                       DEFAULT_DISCOVERY_WHITE_LIST );   
                }
                 if(buffer[0] == 0x02)
                {
                  NPI_PrintString("connecting...\r\n");
                  char *peerAddr;
                  uint8 addrType;
                  // connect to current device in scan result
                 // peerAddr = {0x5F,0x15,0x00,0x01,0x35,0xEF};
                /*  peerAddr[0] = 0x5F;
                  peerAddr[1] = 0x15;
                  peerAddr[2] = 0x00;
                  peerAddr[3] = 0x01;
                  peerAddr[4] = 0x35;
                  peerAddr[5] = 0xEF;*/
                  peerAddr[5] = 0x88;
                  peerAddr[4] = 0x38;
                  peerAddr[3] = 0x00;
                  peerAddr[2] = 0x01;
                  peerAddr[1] = 0x35;
                  peerAddr[0] = 0xEF;
                      
        addrType = 0;
      
        simpleBLEState = BLE_STATE_CONNECTING;
        
        GAPCentralRole_EstablishLink( DEFAULT_LINK_HIGH_DUTY_CYCLE,
                                      DEFAULT_LINK_WHITE_LIST,
                                      addrType, peerAddr );
        
        
     //   GATT_DiscAllCharDescs(simpleBLEConnHandle,12,15,simpleBLETaskId);
                }
                
                if(buffer[0] == 0x03)
                {
                  char status;
                   attWriteReq_t req;
        
                req.pValue = GATT_bm_alloc( simpleBLEConnHandle, ATT_WRITE_REQ, 3, NULL );
                if ( req.pValue != NULL )
                {
                  NPI_PrintString("writing...\r\n");
                  
                  
                  req.handle = 0x12;
                  req.len = 2;
                  req.pValue[0] = 0x01;
                  req.pValue[1] = 0x00;
                  
                  req.sig = 0;
                  req.cmd = 0;
                  status = GATT_WriteCharValue( simpleBLEConnHandle, &req, simpleBLETaskId );
                  
                  if ( status != SUCCESS )
                  {
                    GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
                  }
                  else
                  {
                    NPI_PrintString("write ok\r\n");
                  }
                
                }
                }
                if(buffer[0] == 0x04)
                {
                  char status;
                   attWriteReq_t req;
        
                req.pValue = GATT_bm_alloc( simpleBLEConnHandle, ATT_WRITE_REQ, 3, NULL );
                if ( req.pValue != NULL )
                {
                  NPI_PrintString("writing...\r\n");
                  
              
                  req.handle = 0x15;
                  req.len = 3;
                  req.pValue[0] = 0x00;
                  req.pValue[1] = 0x07;
                  req.pValue[2] = 0x02;
                  req.sig = 0;
                  req.cmd = 0;
                  status = GATT_WriteCharValue( simpleBLEConnHandle, &req, simpleBLETaskId );
                  if ( status != SUCCESS )
                  {
                    GATT_bm_free( (gattMsg_t *)&req, ATT_WRITE_REQ );
                  }
                  else
                  {
                    NPI_PrintString("write ok\r\n");
                  }
                }
                
                    //���յ������ݷ��͵�����-ʵ�ֻػ�   
               // NPI_WriteTransport(buffer, numBytes); 
              }  
                //�ͷ�����Ļ�����  
                osal_mem_free(buffer);  
              
        }  
    } 
    }
}  
/*********************************************************************
*********************************************************************/
