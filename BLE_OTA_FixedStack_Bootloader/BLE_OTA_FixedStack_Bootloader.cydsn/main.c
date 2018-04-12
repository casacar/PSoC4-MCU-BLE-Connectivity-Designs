/*******************************************************************************
* File Name: main.c
*
* Version: 1.40
*
* Description:
*  This example shows how to use custom linker scripts to
*  share a block of memory between bootloader and bootloadable projects.
*  It demonstrates how the bootloader can place API functions such that the
*  bootloadable can also call them. This allows creation of 
*  over-the-air bootloader.
*
* References:
*  BLUETOOTH SPECIFICATION Version 4.1
*
* Hardware Dependency:
*  CY8CKIT-042 BLE
*
********************************************************************************
* Copyright 2014-2016, Cypress Semiconductor Corporation. All rights reserved.
* This software is owned by Cypress Semiconductor Corporation and is protected
* by and subject to worldwide patent and copyright laws and treaties.
* Therefore, you may use this software only as provided in the license agreement
* accompanying the software package from which you obtained this software.
* CYPRESS AND ITS SUPPLIERS MAKE NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* WITH REGARD TO THIS SOFTWARE, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT,
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*******************************************************************************/

#include "main.h"

CYBLE_CONN_HANDLE_T connHandle;

#if defined(__ARMCC_VERSION)
    static unsigned long keep_me __attribute__((used));
#endif /* defined(__ARMCC_VERSION) */
static void LowPowerImplementation(void);


/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  Starts BLE component and performs all configuration changes required for
*  BLE and Bootloader components operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
int main()
{
    const char8 serialNumber[] = "123456";
    
#if defined(__ARMCC_VERSION)    
    keep_me = Image$$DATA$$ZI$$Limit;
    CyReturnToBootloaddableAddress = 0u;
#endif /*__ARMCC_VERSION*/    


    packetRXFlag = 0u;
    UART_Start();
    
    DBG_PRINT_TEXT("\r\n");
    DBG_PRINT_TEXT("\r\n");
    DBG_PRINT_TEXT("> BLE OTA Fixed Stack Bootloader     Application Started\r\n");
    DBG_PRINT_TEXT("> Version: 1.40\r\n");
    DBG_PRINT_TEXT("> Compile Date and Time : " __DATE__ " " __TIME__ "\r\n");
    DBG_PRINT_TEXT("\r\n");
    DBG_PRINT_TEXT("\r\n");

    CyGlobalIntEnable;

    Bootloading_LED_Write(LED_OFF);
    Advertising_LED_1_Write(LED_OFF);
    Advertising_LED_2_Write(LED_OFF);

    
    CyBle_Start(AppCallBack);
    CyBle_GattsEnableAttribute(cyBle_btss.btServiceHandle); /* fix CDT 243443, enabling Bootloader service */
    
    /* Set Serial Number string not initialized in GUI */
    CyBle_DissSetCharacteristicValue(CYBLE_DIS_SERIAL_NUMBER, sizeof(serialNumber), (uint8 *)serialNumber);

    CyBle_GattsDisableAttribute(cyBle_hidss[0].serviceHandle);
    CyBle_GattsDisableAttribute(cyBle_diss.serviceHandle);
    CyBle_GattsDisableAttribute(cyBle_bass[0].serviceHandle);
    CyBle_GattsDisableAttribute(cyBle_scpss.serviceHandle);

    /* Force client to rediscover services in range of bootloader service */
    WriteAttrServChanged();
    
    WDT_Start();

    while(1u == 1u)
    {
        /* CyBle_ProcessEvents() allows BLE stack to process pending events */
        CyBle_ProcessEvents();

        /* To achieve low power in the device */
        LowPowerImplementation();

        /* Handle blue led blinking */
        HandleLeds();
        
        Bootloader_Start();
    }
}


/*******************************************************************************
* Function Name: AppCallBack()
********************************************************************************
*
* Summary:
*   This function handles events that are generated by BLE stack.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void AppCallBack(uint32 event, void* eventParam)
{
    CYBLE_API_RESULT_T apiResult;
    CYBLE_GAP_CONN_UPDATE_PARAM_T connUpdateParam;
    
    switch (event)
    {
        /**********************************************************
        *                       General Events
        ***********************************************************/
        case CYBLE_EVT_STACK_ON: /* This event received when component is Started */
            /* Enter into discoverable mode so that remote can search it. */
            apiResult = CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
            if(apiResult != CYBLE_ERROR_OK)
            {
            }
            break;
        case CYBLE_EVT_HARDWARE_ERROR:    /* This event indicates that some internal HW error has occurred. */
            DBG_PRINTF("CYBLE_EVT_HARDWARE_ERROR\r\n");
            break;
            

        /**********************************************************
        *                       GAP Events
        ***********************************************************/
        case CYBLE_EVT_GAP_AUTH_REQ:
            DBG_PRINTF("CYBLE_EVT_AUTH_REQ: security=%x, bonding=%x, ekeySize=%x, err=%x \r\n",
                (*(CYBLE_GAP_AUTH_INFO_T *)eventParam).security,
                (*(CYBLE_GAP_AUTH_INFO_T *)eventParam).bonding,
                (*(CYBLE_GAP_AUTH_INFO_T *)eventParam).ekeySize,
                (*(CYBLE_GAP_AUTH_INFO_T *)eventParam).authErr);
            break;
        case CYBLE_EVT_GAP_PASSKEY_ENTRY_REQUEST:
            DBG_PRINTF("CYBLE_EVT_PASSKEY_ENTRY_REQUEST press 'p' to enter passkey \r\n");
            break;
        case CYBLE_EVT_GAP_PASSKEY_DISPLAY_REQUEST:
            DBG_PRINTF("CYBLE_EVT_PASSKEY_DISPLAY_REQUEST %6.6ld \r\n", *(uint32 *)eventParam);
            break;
        case CYBLE_EVT_GAP_KEYINFO_EXCHNGE_CMPLT:
            DBG_PRINTF("CYBLE_EVT_GAP_KEYINFO_EXCHNGE_CMPLT \r\n");
            break;
        case CYBLE_EVT_GAP_AUTH_COMPLETE:
            DBG_PRINTF("AUTH_COMPLETE \r\n");
            break;
        case CYBLE_EVT_GAP_AUTH_FAILED:
            DBG_PRINTF("CYBLE_EVT_AUTH_FAILED: %x \r\n", *(uint8 *)eventParam);
            break;
        case CYBLE_EVT_GAP_DEVICE_CONNECTED:
            DBG_PRINTF("CYBLE_EVT_GAP_DEVICE_CONNECTED: %d \r\n", connHandle.bdHandle);
            if ((*(CYBLE_GAP_CONN_PARAM_UPDATED_IN_CONTROLLER_T *)eventParam).connIntv > 0x0006u)
            {
                /* If connection settings do not match expected ones - request parameter update */
                connUpdateParam.connIntvMin   = 0x0006u;
                connUpdateParam.connIntvMax   = 0x0006u;
                connUpdateParam.connLatency   = 0x0000u;
                connUpdateParam.supervisionTO = 0x0064u;
                apiResult = CyBle_L2capLeConnectionParamUpdateRequest(cyBle_connHandle.bdHandle, &connUpdateParam);
                DBG_PRINTF("CyBle_L2capLeConnectionParamUpdateRequest API: 0x%2.2x \r\n", apiResult);
            }
            Bootloading_LED_Write(LED_OFF);
            break;
        case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
            DBG_PRINTF("CYBLE_EVT_GAP_DEVICE_DISCONNECTED\r\n");
            apiResult = CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
            if(apiResult != CYBLE_ERROR_OK)
            {
                DBG_PRINTF("StartAdvertisement API Error: %d \r\n", apiResult);
            }
            break;
        case CYBLE_EVT_GAP_ENCRYPT_CHANGE:
            DBG_PRINTF("CYBLE_EVT_GAP_ENCRYPT_CHANGE: %x \r\n", *(uint8 *)eventParam);
            break;
        case CYBLE_EVT_GAPC_CONNECTION_UPDATE_COMPLETE:
            DBG_PRINTF("CYBLE_EVT_CONNECTION_UPDATE_COMPLETE: %x \r\n", *(uint8 *)eventParam);
            break;
        case CYBLE_EVT_GAPP_ADVERTISEMENT_START_STOP:
            if(CYBLE_STATE_DISCONNECTED == CyBle_GetState())
            {   
                /* Fast and slow advertising period complete, go to low power  
                 * mode (Hibernate mode) and wait for an external
                 * user event to wake up the device again */
                DBG_PRINTF("Entering low power mode...\r\n");
                Bootloading_LED_Write(LED_ON);
                Advertising_LED_1_Write(LED_ON);
                Advertising_LED_2_Write(LED_ON);
                Bootloader_Service_Activation_ClearInterrupt();
                Wakeup_Interrupt_ClearPending();
                Wakeup_Interrupt_Start();
                CySysPmHibernate();
            }
            break;

            
        /**********************************************************
        *                       GATT Events
        ***********************************************************/
        case CYBLE_EVT_GATT_CONNECT_IND:
            connHandle = *(CYBLE_CONN_HANDLE_T *)eventParam;
            break;
        case CYBLE_EVT_GATT_DISCONNECT_IND:
            connHandle.bdHandle = 0;
            break;
        case CYBLE_EVT_GATTS_WRITE_CMD_REQ:
            DBG_PRINTF("CYBLE_EVT_GATTS_WRITE_CMD_REQ\r\n");
            break;
        case CYBLE_EVT_GATTS_PREP_WRITE_REQ:
            (void)CyBle_GattsPrepWriteReqSupport(CYBLE_GATTS_PREP_WRITE_NOT_SUPPORT);
            break;
        case CYBLE_EVT_HCI_STATUS:
            DBG_PRINTF("CYBLE_EVT_HCI_STATUS\r\n");
        default:
            break;
        }
}


/*******************************************************************************
* Function Name: WriteAttrServChanged()
********************************************************************************
*
* Summary:
*   Sets serviceChangedHandle for enabling or disabling hidden service.
*
* Parameters:
*   None
*
* Return:
*   None
*
*******************************************************************************/
void WriteAttrServChanged(void)
{
    uint32 value;
    CYBLE_GATT_HANDLE_VALUE_PAIR_T    handleValuePair;
    
    /* Force client to rediscover services in range of bootloader service */
    value = ((uint32)(((uint32) cyBle_btss.btServiceHandle) << 16u)) | 
        ((uint32) (cyBle_btss.btServiceInfo[0u].btServiceCharDescriptors[0u]));

    handleValuePair.value.val = (uint8 *)&value;
    handleValuePair.value.len = sizeof(value);

    handleValuePair.attrHandle = cyBle_gatts.serviceChangedHandle;
    CyBle_GattsWriteAttributeValue(&handleValuePair, 0u, NULL,CYBLE_GATT_DB_LOCALLY_INITIATED);
}


/*******************************************************************************
* Function Name: LowPowerImplementation()
********************************************************************************
* Summary:
* Implements low power in the project.
*
* Parameters:
* None
*
* Return:
* None
*
* Theory:
* The function tries to enter deep sleep as much as possible - whenever the 
* BLE is idle and the UART transmission/reception is not happening. At all other
* times, the function tries to enter CPU sleep.
*
*******************************************************************************/
static void LowPowerImplementation(void)
{
    CYBLE_LP_MODE_T bleMode;
    uint8 interruptStatus;
    
    /* For advertising and connected states, implement deep sleep 
     * functionality to achieve low power in the system. For more details
     * on the low power implementation, refer to the Low Power Application 
     * Note.
     */
    if((CyBle_GetState() == CYBLE_STATE_ADVERTISING) || 
       (CyBle_GetState() == CYBLE_STATE_CONNECTED))
    {
        /* Request BLE subsystem to enter into Deep-Sleep mode between connection and advertising intervals */
        bleMode = CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP);
        /* Disable global interrupts */
        interruptStatus = CyEnterCriticalSection();
        /* When BLE subsystem has been put into Deep-Sleep mode */
        if(bleMode == CYBLE_BLESS_DEEPSLEEP)
        {
            /* And it is still there or ECO is on */
            if((CyBle_GetBleSsState() == CYBLE_BLESS_STATE_ECO_ON) || 
               (CyBle_GetBleSsState() == CYBLE_BLESS_STATE_DEEPSLEEP))
            {
                CySysPmDeepSleep();
                
                /* Handle advertising led blinking */
                HandleLeds();
            }
        }
        else /* When BLE subsystem has been put into Sleep mode or is active */
        {
            /* And hardware doesn't finish Tx/Rx opeation - put the CPU into Sleep mode */
            if(CyBle_GetBleSsState() != CYBLE_BLESS_STATE_EVENT_CLOSE)
            {
                CySysPmSleep();
            }
        }
        /* Enable global interrupt */
        CyExitCriticalSection(interruptStatus);
    }
}


/* [] END OF FILE */
