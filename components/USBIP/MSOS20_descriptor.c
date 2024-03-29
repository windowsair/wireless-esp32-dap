/**
 * @file MSOS20_descriptor.c
 * @author windowsair
 * @brief Store related data of Microsoft OS 2.0 descriptor
 * @version 0.1
 * @date 2019-11-21
 *
 * @copyright Copyright (c) 2019
 *
 */

 ////TODO: refactoring into structure

#include <stdint.h>

#include "components/USBIP/MSOS20_descriptor.h"

#define USBShort(ui16Value)     ((ui16Value) & 0xff), ((ui16Value) >> 8) //((ui16Value) & 0xFF),(((ui16Value) >> 8) & 0xFF)



// Microsoft OS 2.0 descriptor set header
const uint8_t msOs20DescriptorSetHeader[kLengthOfMsOS20] =
{
    // Microsoft OS 2.0 Descriptor Set header (Table 10)
    0x0A, 0x00,  // wLength (Shall be set to 0x0A)
    MS_OS_20_SET_HEADER_DESCRIPTOR, 0x00,
    0x00, 0x00, 0x03, 0x06,  // dwWindowsVersion: Windows 8.1 (NTDDI_WINBLUE)
    USBShort(kLengthOfMsOS20),  // wTotalLength

    // Support WinUSB
    // See https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/automatic-installation-of-winusb

    // Microsoft OS 2.0 compatible ID descriptor (Table 13)
    0x14, 0x00,                                      // wLength
    USBShort(MS_OS_20_FEATURE_COMPATIBLE_ID),        // wDescriptorType
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,        // compatibleID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // subCompatibleID

    // Microsoft OS 2.0 registry property descriptor (Table 14)
    0x84, 0x00,                                      // wLength
    USBShort(MS_OS_20_FEATURE_REG_PROPERTY),
    0x07, 0x00,                                      // wPropertyDataType: REG_MULTI_SZ (Unicode Strings)
    0x2A, 0x00,                                      // wPropertyNameLength
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,'I',0,'n',0,'t',0,'e',0,'r',0,
    'f',0,'a',0,'c',0,'e',0,'G',0,'U',0,'I',0,'D',0,'s',0,0,0,
                                                     // Set to "DeviceInterfaceGUID" to support WinUSB
    0x50, 0x00,                                      // wPropertyDataLength
                                                     // WinUSB GUID
    '{',0,'C',0,'D',0,'B',0,'3',0,'B',0,'5',0,'A',0,'D',0,'-',0,
    '2',0,'9',0,'3',0,'B',0,'-',0,'4',0,'6',0,'6',0,'3',0,'-',0,
    'A',0,'A',0,'3',0,'6',0,'-',0,'1',0,'A',0,'A',0,'E',0,'4',0,
    '6',0,'4',0,'6',0,'3',0,'7',0,'7',0,'6',0,'}',0,0,0,0,0,
    // identify a CMSIS-DAP V2 configuration,
    // must set to "{CDB3B5AD-293B-4663-AA36-1AAE46463776}"

};

const uint8_t bosDescriptor[kLengthOfBos] =
{
    // Universal Serial Bus 3.0 Specification, Table 9-9.
    0x05,                       // bLength of this descriptor
    USB_DESCRIPTOR_TYPE_BOS,    // BOS Descriptor type(Constant)
    USBShort(kLengthOfBos),     // wLength
    0x03,                       // bNumDeviceCaps -> only 0x01 for OS2.0 descriptor

    // USB 2.0 extension
    0x07, /* Descriptor size */
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY, /* Device capability type descriptor */
    USB_DEVICE_CAPABILITY_TYPE_USB2_0_EXTENSION, /* USB 2.0 extension capability type */
    0x02, 0x00, 0x00, 0x00, /* Supported device level features: LPM support  */

    /* SuperSpeed device capability */
    0x0A, /* Descriptor size */
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY, /* Device capability type descriptor */
    USB_DEVICE_CAPABILITY_TYPE_SUPERSPEED_USB, /* SuperSpeed device capability type */
    0x00, /* Supported device level features(LTM Capable)  */
    0x08, 0x00, /* Speeds supported by the device : SS, ~~HS~ and ~~FS~~ */
    0x03, /* Functionality support */
    0x00, /* U1 Device Exit latency */
    0x00, 0x00, /* U2 Device Exit latency */




    // Microsoft OS 2.0 platform capability descriptor header (Table 4)
    // See also:
    // Universal Serial Bus 3.0 Specification : Format of a Device Capability Descriptor, Table 9-10.

    0x1C,       // bLength of this first device capability descriptor
                // bLength -> The total length of the remaining arrays containing this field
    USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY, // bDescriptorType
    USB_DEVICE_CAPABILITY_TYPE_PLATFORM, // bDevCapabilityType

    // Capability-Dependent (See USB3.0 Specification Table 9-10.)
    0x00,                       // bReserved
    USB_DEVICE_CAPABILITY_UUID, // MS_OS_20_Platform_Capability_ID

    0x00, 0x00, 0x03, 0x06,     // dwWindowsVersion: Windows 8.1 (NTDDI_WINBLUE)
    USBShort(kLengthOfMsOS20),  // wMSOSDescriptorSetTotalLength(length of descriptor set header)
    kValueOfbMS_VendorCode,     // bMS_VendorCode (0x01 will be ok)
                                ////TODO:change this
    0,                          // bAltEnumCode
};