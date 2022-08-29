/**
 * @file DAP_handle.c
 * @brief Handle DAP packets and transaction push
 * @version 0.4
 * @change: 2020.02.04 first version
 *          2020.11.11 support WinUSB mode
 *          2021.02.17 support SWO
 *
 * @copyright Copyright (c) 2021
 *
 */

#include <stdint.h>
#include <string.h>

#include "main/usbip_server.h"
#include "main/DAP_handle.h"
#include "main/dap_configuration.h"

#include "components/USBIP/USB_descriptor.h"
#include "components/DAP/include/DAP.h"
//#include "swo.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define DISABLE_CPU1_INTR 0

extern int kSock;
extern TaskHandle_t kDAPTaskHandle;

static portMUX_TYPE my_mutex;

int kRestartDAPHandle = 0;


#if (USE_WINUSB == 1)
typedef struct
{
    uint32_t length;
    uint8_t buf[DAP_PACKET_SIZE];
} DAPPacetDataType;
#else
typedef struct
{
    uint8_t buf[DAP_PACKET_SIZE];
} DAPPacetDataType;
#endif


#define DAP_HANDLE_SIZE (sizeof(DAPPacetDataType))

static DAPPacetDataType DAPDataProcessed;

#if (DISABLE_CPU1_INTR == 1)
static _Atomic int dap_respond = 0;
#else
static int dap_respond = 0;
#endif


// SWO Trace
static uint8_t *swo_data_to_send = NULL;
static uint32_t swo_data_num;



// DAP handle
static RingbufHandle_t dap_dataIN_handle = NULL;
static RingbufHandle_t dap_dataOUT_handle = NULL;
static SemaphoreHandle_t data_response_mux = NULL;

static void unpack(void *data, int size);

void handle_dap_data_request(usbip_stage2_header *header, uint32_t length)
{
    uint8_t *data_in = (uint8_t *)header;
    data_in = &(data_in[sizeof(usbip_stage2_header)]);
    // Point to the beginning of the URB packet

#if (USE_WINUSB == 1)
    send_stage2_submit(header, 0, 0);

    // always send constant size buf -> cuz we don't care about the IN packet size
    // and to unify the style, we set aside the length of the section
    xRingbufferSend(dap_dataIN_handle, data_in - sizeof(uint32_t), DAP_HANDLE_SIZE, portMAX_DELAY);
    xTaskNotifyGive(kDAPTaskHandle);

#else
    send_stage2_submit(header, 0, 0);

    xRingbufferSend(dap_dataIN_handle, data_in, DAP_HANDLE_SIZE, portMAX_DELAY);
    xTaskNotifyGive(kDAPTaskHandle);

#endif

    // dap_respond = DAP_ProcessCommand((uint8_t *)data_in, (uint8_t *)data_out);
    // //handle_dap_data_response(header);
    // send_stage2_submit(header, 0, 0);
}

void handle_dap_data_response(usbip_stage2_header *header)
{
    return;
    // int resLength = dap_respond & 0xFFFF;
    // if (resLength)
    // {

    //     send_stage2_submit_data(header, 0, (void *)DAPDataProcessed.buf, resLength);
    //     dap_respond = 0;
    // }
    // else
    // {
    //     send_stage2_submit(header, 0, 0);
    // }
}

void handle_swo_trace_response(usbip_stage2_header *header)
{
#if (SWO_FUNCTION_ENABLE == 1)
    if (kSwoTransferBusy)
    {
        // busy indicates that there is data to be send
        printf("swo use data\r\n");
        send_stage2_submit_data(header, 0, (void *)swo_data_to_send, swo_data_num);
        SWO_TransferComplete();
    }
    else
    {
        // nothing to send.
        send_stage2_submit(header, 0, 0);
    }
#else
    send_stage2_submit(header, 0, 0);
#endif
}

// SWO Data Queue Transfer
//   buf:    pointer to buffer with data
//   num:    number of bytes to transfer
void SWO_QueueTransfer(uint8_t *buf, uint32_t num)
{
    swo_data_to_send = buf;
    swo_data_num = num;
}

IRAM_ATTR void DAP_Thread(void *argument)
{
    // vPortCPUInitializeMutex(&my_mutex);
    // portENTER_CRITICAL(&my_mutex);
    //portDISABLE_INTERRUPTS();

    dap_dataIN_handle = xRingbufferCreate(DAP_HANDLE_SIZE * 20, RINGBUF_TYPE_BYTEBUF);
    dap_dataOUT_handle = xRingbufferCreate(368 * 20, RINGBUF_TYPE_BYTEBUF);
    data_response_mux = xSemaphoreCreateMutex();
    size_t packetSize;
    int resLength;
    DAPPacetDataType *item;

    if (dap_dataIN_handle == NULL || dap_dataIN_handle == NULL ||
        data_response_mux == NULL)
    {
        printf("Can not create DAP ringbuf/mux!\r\n");
        vTaskDelete(NULL);
    }

#if (DISABLE_CPU1_INTR == 1)
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    vPortCPUInitializeMutex(&my_mutex);
    portENTER_CRITICAL(&my_mutex);
    goto start;
#endif

    for (;;)
    {

        while (1)
        {
            if (kRestartDAPHandle)
            {
                vRingbufferDelete(dap_dataIN_handle);
                vRingbufferDelete(dap_dataOUT_handle);
                dap_dataIN_handle = dap_dataOUT_handle = NULL;

                dap_dataIN_handle = xRingbufferCreate(DAP_HANDLE_SIZE * 20, RINGBUF_TYPE_BYTEBUF);
                dap_dataOUT_handle = xRingbufferCreate(DAP_HANDLE_SIZE * 20, RINGBUF_TYPE_BYTEBUF);
                if (dap_dataIN_handle == NULL || dap_dataIN_handle == NULL)
                {
                    printf("Can not create DAP ringbuf/mux!\r\n");
                    vTaskDelete(NULL);
                }
                kRestartDAPHandle = 0;
            }

            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
start:
            packetSize = 0;
            item = (DAPPacetDataType *)xRingbufferReceiveUpTo(dap_dataIN_handle, &packetSize,
                                                          0, DAP_HANDLE_SIZE);
            if (packetSize == 0)
            {
                break;
            }


            if (item->buf[0] == ID_DAP_QueueCommands)
            {
                item->buf[0] = ID_DAP_ExecuteCommands;
            }

            resLength = DAP_ProcessCommand((uint8_t *)item->buf, (uint8_t *)DAPDataProcessed.buf); // use first 4 byte to save length
            resLength &= 0xFFFF; // res length in lower 16 bits

            vRingbufferReturnItem(dap_dataIN_handle, (void *)item); // process done.

            // now prepare to reply
        #if (USE_WINUSB == 1)
            DAPDataProcessed.length = resLength;
        #endif
            xRingbufferSend(dap_dataOUT_handle, (void *)&DAPDataProcessed, 368, portMAX_DELAY);


#if (DISABLE_CPU1_INTR == 1)
            if (xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
            {
                ++dap_respond;
                xSemaphoreGive(data_response_mux);
            }
#else
            ++dap_respond;
#endif

        }
    }
}

int fast_reply(uint8_t *buf, uint32_t length)
{
    if (length == 48 && buf[3] == 1 && buf[15] == 1 && buf[19] == 1)
    {
        if (dap_respond > 0)
        {
            DAPPacetDataType *item;
            size_t packetSize = 0;
            item = (DAPPacetDataType *)xRingbufferReceiveUpTo(dap_dataOUT_handle, &packetSize,
                                                     (10 / portTICK_RATE_MS), 368);
            if (packetSize == 368)
            {
                unpack((uint32_t *)buf, sizeof(usbip_stage2_header));

            #if (USE_WINUSB == 1)
                uint32_t resLength = item->length;
                send_stage2_submit_data_fast((usbip_stage2_header *)buf, 0, item->buf, resLength);
            #else
                send_stage2_submit_data_fast((usbip_stage2_header *)buf, 0, item->buf, DAP_HANDLE_SIZE);
            #endif

#if (DISABLE_CPU1_INTR == 1)
                if (xSemaphoreTake(data_response_mux, portMAX_DELAY) == pdTRUE)
                {
                    --dap_respond;
                    xSemaphoreGive(data_response_mux);
                }
#else
                --dap_respond;
#endif
                vRingbufferReturnItem(dap_dataOUT_handle, (void *)item);
                return 1;
            }
            else if (packetSize > 0)
            {
                printf("Wrong data out packet size:%d!\r\n", packetSize);
            }
            ////TODO: fast reply
        }
        else
        {
            //// TODO: ep0 dir 0 ?
            buf[0x3] = 0x3; // command
            buf[0xF] = 0;  // direction
            buf[0x16] = 0;
            buf[0x17] = 0;
            buf[27] = 0;
            buf[39] = 0;
            send(kSock, buf, 48, 0);
            return 1;
        }
    }
    return 0;
}

static void unpack(void *data, int size)
{
    // Ignore the setup field
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {
        ptr[i] = ntohl(ptr[i]);
    }
}