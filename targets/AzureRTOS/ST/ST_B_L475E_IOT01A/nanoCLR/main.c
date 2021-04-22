//
// Copyright (c) .NET Foundation and Contributors
// See LICENSE file in the project root for full license information.
//

#include <nanoCLR_Headers.h>
#include <stm32l4xx_hal.h>
#include <stm32l475e_iot01.h>

#include <cmsis_utils.h>
#include <tx_api.h>

#include <LaunchCLR.h>
#include <targetHAL.h>
#include <nanoPAL_BlockStorage.h>
#include <nanoHAL_ConfigurationManager.h>
#include <nanoCLR_Application.h>
#include <nanoHAL_v2.h>

void BoardInit(bool initSensors, bool initGpios);

extern TX_EVENT_FLAGS_GROUP wpUartEvent;
extern CLR_SETTINGS clrSettings;

// byte pool configuration and definitions
#define DEFAULT_BYTE_POOL_SIZE 4096
TX_BYTE_POOL byte_pool_0;
uint8_t memory_area[DEFAULT_BYTE_POOL_SIZE];

// threads definitions and configurations

// receiver thread
#define RECEIVER_THREAD_STACK_SIZE 2048
#define RECEIVER_THREAD_PRIORITY   5

TX_THREAD receiverThread;
uint32_t receiverThreadStack[RECEIVER_THREAD_STACK_SIZE / sizeof(uint32_t)];
extern void ReceiverThread_entry(uint32_t parameter);

// CLR thread
#define CLR_THREAD_STACK_SIZE 4096
#define CLR_THREAD_PRIORITY   5

TX_THREAD clrStartupThread;
uint32_t clrStartupThreadStack[CLR_THREAD_STACK_SIZE / sizeof(uint32_t)];
extern void ClrStartupThread_entry(uint32_t parameter);

void tx_application_define(void *first_unused_memory)
{
    (void)first_unused_memory;
    uint16_t status;

    systick_interval_set(TX_TIMER_TICKS_PER_SECOND);

    // Create a byte memory pool from which to allocate the thread stacks.
    tx_byte_pool_create(&byte_pool_0, "byte pool 0", memory_area, DEFAULT_BYTE_POOL_SIZE);

    // initialize block storage list and devices
    // in CLR this is called in nanoHAL_Initialize()
    // for nanoBooter we have to init it in order to provide the flash map for Monitor_FlashSectorMap command
    BlockStorageList_Initialize();
    BlockStorage_AddDevices();

    // initialize configuration manager
    // in CLR this is called in nanoHAL_Initialize()
    // for nanoBooter we have to init it here to have access to network configuration blocks
    ConfigurationManager_Initialize();

    // Create receiver thread
    status = tx_thread_create(
        &receiverThread,
        "Receiver Thread",
        ReceiverThread_entry,
        0,
        receiverThreadStack,
        RECEIVER_THREAD_STACK_SIZE,
        RECEIVER_THREAD_PRIORITY,
        RECEIVER_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        while (1)
        {
        }
    }

    // CLR settings to launch CLR thread
    (void)memset(&clrSettings, 0, sizeof(CLR_SETTINGS));

    clrSettings.MaxContextSwitches = 50;
    clrSettings.WaitForDebugger = false;
    clrSettings.EnterDebuggerLoopAfterExit = true;

    // Create CLR startup thread
    status = tx_thread_create(
        &clrStartupThread,
        "CLR Thread",
        ClrStartupThread_entry,
        (uint32_t)&clrSettings,
        clrStartupThreadStack,
        CLR_THREAD_STACK_SIZE,
        CLR_THREAD_PRIORITY,
        CLR_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        while (1)
        {
        }
    }

    // create UART event group
    status = tx_event_flags_create(&wpUartEvent, "wpUart event");
    if (status != TX_SUCCESS)
    {
        while (1)
        {
        }
    }
}

//  Application entry point.
int main(void)
{
    // init board WITH sensors and WITHOUT GPIOs
    BoardInit(false, false);

    // init boot clipboard
    InitBootClipboard();

    // Enter the ThreadX kernel
    tx_kernel_enter();
}
