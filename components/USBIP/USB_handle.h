#ifndef __USB_HANDLE_H__
#define __USB_HANDLE_H__

#include "components/USBIP/USBIP_defs.h"

void handleUSBControlRequest(usbip_stage2_header *header);

#endif