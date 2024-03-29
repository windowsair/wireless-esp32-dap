#include <stdint.h>
#include <string.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "components/USBIP/USB_handle.h"
#include "components/USBIP/USB_descriptor.h"

#include "main/usbip_server.h"
#include "main/dap_handle.h"


// attach helper function
static int read_stage1_command(uint8_t *buffer, uint32_t length);
static void handle_device_list(uint8_t *buffer, uint32_t length);
static void handle_device_attach(uint8_t *buffer, uint32_t length);
static void send_stage1_header(uint16_t command, uint32_t status);
static void send_device_list();
static void send_device_info();
static void send_interface_info();

// emulate helper function
static void pack(void *data, int size);
static void unpack(void *data, int size);
static int handle_submit(usbip_stage2_header *header, uint32_t length);
static int read_stage2_command(usbip_stage2_header *header, uint32_t length);

static void handle_unlink(usbip_stage2_header *header);
// unlink helper function
static void send_stage2_unlink(usbip_stage2_header *req_header);



int attach(uint8_t *buffer, uint32_t length)
{
    int command = read_stage1_command(buffer, length);
    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE1_CMD_DEVICE_LIST: // OP_REQ_DEVLIST
        handle_device_list(buffer, length);
        break;

    case USBIP_STAGE1_CMD_DEVICE_ATTACH: // OP_REQ_IMPORT
        handle_device_attach(buffer, length);
        break;

    default:
        printf("attach Unknown command: %d\r\n", command);
        break;
    }
    return 0;
}

static int read_stage1_command(uint8_t *buffer, uint32_t length)
{
    if (length < sizeof(usbip_stage1_header))
    {
        return -1;
    }
    usbip_stage1_header *req = (usbip_stage1_header *)buffer;
    return (ntohs(req->command) & 0xFF); // 0x80xx low bit
}

static void handle_device_list(uint8_t *buffer, uint32_t length)
{
    printf("Handling dev list request...\r\n");
    send_stage1_header(USBIP_STAGE1_CMD_DEVICE_LIST, 0);
    send_device_list();
}

static void handle_device_attach(uint8_t *buffer, uint32_t length)
{
    printf("Handling dev attach request...\r\n");

    //char bus[USBIP_BUSID_SIZE];
    if (length < sizeof(USBIP_BUSID_SIZE))
    {
        printf("handle device attach failed!\r\n");
        return;
    }
    //client.readBytes((uint8_t *)bus, USBIP_BUSID_SIZE);

    send_stage1_header(USBIP_STAGE1_CMD_DEVICE_ATTACH, 0);

    send_device_info();

    kState = EMULATING;
}

static void send_stage1_header(uint16_t command, uint32_t status)
{
    printf("Sending header...\r\n");
    usbip_stage1_header header;
    header.version = htons(273); ////TODO:  273???
    // may be : https://github.com/Oxalin/usbip_windows/issues/4

    header.command = htons(command);
    header.status = htonl(status);

    send(kSock, (uint8_t *)&header, sizeof(usbip_stage1_header), 0);
}

static void send_device_list()
{
    printf("Sending device list...\r\n");

    // send device list size:
    printf("Sending device list size...\r\n");
    usbip_stage1_response_devlist response_devlist;

    // we have only 1 device, so:
    response_devlist.list_size = htonl(1);

    send(kSock, (uint8_t *)&response_devlist, sizeof(usbip_stage1_response_devlist), 0);

    // may be foreach:

    {
        // send device info:
        send_device_info();
        // send device interfaces: // (1)
        send_interface_info();
    }
}

static void send_device_info()
{
    printf("Sending device info...\r\n");
    usbip_stage1_usb_device device;

    strcpy(device.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
    strcpy(device.busid, "1-1");

    device.busnum = htonl(1);
    device.devnum = htonl(1);
    device.speed = htonl(3); // See usb_device_speed enum

    device.idVendor = htons(USBD0_DEV_DESC_IDVENDOR);
    device.idProduct = htons(USBD0_DEV_DESC_IDPRODUCT);
    device.bcdDevice = htons(USBD0_DEV_DESC_BCDDEVICE);

    device.bDeviceClass = 0x00; // We need to use a device other than the USB-IF standard, set to 0x00
    device.bDeviceSubClass = 0x00;
    device.bDeviceProtocol = 0x00;

    device.bConfigurationValue = 1;
    device.bNumConfigurations = 1;
    device.bNumInterfaces = 1;

    send(kSock, (uint8_t *)&device, sizeof(usbip_stage1_usb_device), 0);
}

static void send_interface_info()
{
    printf("Sending interface info...\r\n");
    usbip_stage1_usb_interface interface;
    interface.bInterfaceClass = USBD_CUSTOM_CLASS0_IF0_CLASS;
    interface.bInterfaceSubClass = USBD_CUSTOM_CLASS0_IF0_SUBCLASS;
    interface.bInterfaceProtocol = USBD_CUSTOM_CLASS0_IF0_PROTOCOL;
    interface.padding = 0; // shall be set to zero

    send(kSock, (uint8_t *)&interface, sizeof(usbip_stage1_usb_interface), 0);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

int emulate(uint8_t *buffer, uint32_t length)
{
    if (length < 48)
    {
        for (int i = 0; i < 100; i++)
        {
            printf("TCP issue!\r\n");
        }
    }

    if(fast_reply(buffer, length))
    {
        return 0;
    }

    int command = read_stage2_command((usbip_stage2_header *)buffer, length);
    if (command < 0)
    {
        return -1;
    }

    switch (command)
    {
    case USBIP_STAGE2_REQ_SUBMIT:
        handle_submit((usbip_stage2_header *)buffer , length);
        break;

    case USBIP_STAGE2_REQ_UNLINK:
        handle_unlink((usbip_stage2_header *)buffer);
        break;

    default:
        printf("emulate unknown command:%d\r\n", command);
        //handle_submit((usbip_stage2_header *)buffer, length);
        return -1;
    }
    return 0;
}

static int read_stage2_command(usbip_stage2_header *header, uint32_t length)
{
    if (length < sizeof(usbip_stage2_header))
    {
        return -1;
    }

    //client.readBytes((uint8_t *)&header, sizeof(usbip_stage2_header));
    unpack((uint32_t *)header, sizeof(usbip_stage2_header));
    return header->base.command;
}

/**
 * @brief Pack the following packets(Offset 0x00 - 0x28):
 *       - cmd_submit
 *       - ret_submit
 *       - cmd_unlink
 *       - ret_unlink
 *
 * @param data Point to packets header
 * @param size Packets header size
 */
static void pack(void *data, int size)
{

    // Ignore the setup field
    int sz = (size / sizeof(uint32_t)) - 2;
    uint32_t *ptr = (uint32_t *)data;

    for (int i = 0; i < sz; i++)
    {

        ptr[i] = htonl(ptr[i]);
    }
}

/**
 * @brief Unack the following packets(Offset 0x00 - 0x28):
 *       - cmd_submit
 *       - ret_submit
 *       - cmd_unlink
 *       - ret_unlink
 *
 * @param data Point to packets header
 * @param size  packets header size
 */
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

/**
 * @brief USB transaction processing
 *
 */
static int handle_submit(usbip_stage2_header *header, uint32_t length)
{
    switch (header->base.ep)
    {
    // control endpoint(endpoint 0)
    case 0x00:
        //// TODO: judge usb setup 8 byte?
        handleUSBControlRequest(header);
        break;

    // endpoint 1 data receicve and response
    case 0x01:
        if (header->base.direction == 0)
        {
            //printf("EP 01 DATA FROM HOST");
            handle_dap_data_request(header ,length);
        }
        else
        {
            // printf("EP 01 DATA TO HOST\r\n");
            handle_dap_data_response(header);
        }
        break;
    // endpoint 2 for SWO trace
    case 0x02:
        if (header->base.direction == 0)
        {
            // printf("EP 02 DATA FROM HOST");
            send_stage2_submit(header, 0, 0);
        }
        else
        {
            // printf("EP 02 DATA TO HOST");
            //// FIXME: esp32
            handle_swo_trace_response(header);
        }
        break;
    // request to save data to device
    case 0x81:
        if (header->base.direction == 0)
        {
            printf("*** WARN! EP 81 DATA TX");
        }
        else
        {
            printf("*** WARN! EP 81 DATA RX");
        }
        return -1;

    default:
        printf("*** WARN ! UNKNOWN ENDPOINT: %d\r\n", (int)header->base.ep);
        return -1;
    }
    return 0;
}

void send_stage2_submit(usbip_stage2_header *req_header, int32_t status, int32_t data_length)
{

    req_header->base.command = USBIP_STAGE2_RSP_SUBMIT;
    req_header->base.direction = !(req_header->base.direction);

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));

    req_header->u.ret_submit.status = status;
    req_header->u.ret_submit.data_length = data_length;

    pack(req_header, sizeof(usbip_stage2_header));
    send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}

void send_stage2_submit_data(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length)
{

    send_stage2_submit(req_header, status, data_length);

    if (data_length)
    {
        send(kSock, data, data_length, 0);
    }
}

void send_stage2_submit_data_fast(usbip_stage2_header *req_header, int32_t status, const void *const data, int32_t data_length)
{
    const uint8_t * send_buf = (uint8_t *)req_header;

    req_header->base.command = USBIP_STAGE2_RSP_SUBMIT;
    req_header->base.direction = !(req_header->base.direction);

    memset(&(req_header->u.ret_submit), 0, sizeof(usbip_stage2_header_ret_submit));

    req_header->u.ret_submit.status = status;
    req_header->u.ret_submit.data_length = data_length;

    pack(req_header, sizeof(usbip_stage2_header));

    // payload
    memcpy(&send_buf[sizeof(usbip_stage2_header)], data, data_length);
    send(kSock, send_buf, sizeof(usbip_stage2_header) + data_length, 0);
}

static void handle_unlink(usbip_stage2_header *header)
{
    printf("s2 handling cmd unlink...\r\n");
    send_stage2_unlink(header);
}
static void send_stage2_unlink(usbip_stage2_header *req_header)
{

    req_header->base.command = USBIP_STAGE2_RSP_UNLINK;
    req_header->base.direction = USBIP_DIR_OUT;

    memset(&(req_header->u.ret_unlink), 0, sizeof(usbip_stage2_header_ret_unlink));

    // req_header.u.ret_unlink.status = 0;

    pack(req_header, sizeof(usbip_stage2_header));

    send(kSock, req_header, sizeof(usbip_stage2_header), 0);
}