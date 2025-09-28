#ifndef USB_H
#define USB_H

#include ".\include\libusb-1.0\libusb.h"
#include "stdio.h"
#include "string.h"

#define VID 1155  // change this VID & PID to communicate with other devices
#define PID 22336
#define DEVICE_CLASS_CODE LIBUSB_CLASS_DATA
#define EP_IN_INDEX 1
#define EP_OUT_INDEX 0

void read_usb(uint8_t buf[], uint16_t len);
void write_usb(uint8_t buf[], uint16_t len);

#endif