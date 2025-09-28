#include "usb.h"

int callback_triggered = 0;
libusb_device *device = NULL;
libusb_device **list = NULL;
struct libusb_interface_descriptor itf_desc;

void usb_callback(struct libusb_transfer *transfer)
{
    callback_triggered = 1;
}

// Get the device descriptor and interface descriptor of the target device
void init_usb() {
    libusb_init(NULL);

    // Discover devices
    ssize_t num_of_devices = libusb_get_device_list(NULL, &list);
    for (ssize_t i = 0; i < num_of_devices; i++) {
        device = list[i];
    
        struct libusb_device_descriptor desc;
        // find the device with the specified VID & PID
        while (libusb_get_device_descriptor(device, &desc) != 0);
        if (desc.idVendor == VID && desc.idProduct == PID) {
            struct libusb_config_descriptor *config_desc;
            // wait until this device has a active configuration descriptor
            while (libusb_get_active_config_descriptor(device, &config_desc) != 0);

            // find an interface and alternate setting having the specified class code
            const struct libusb_interface *interface_list = config_desc->interface;
            uint8_t num_of_inter = config_desc->bNumInterfaces;
            for (int j = 0; j < num_of_inter; ++j) {
                uint8_t num_of_alt = interface_list[j].num_altsetting;
                for (int k = 0; k < num_of_alt; ++k) {
                    if (interface_list[j].altsetting[k].bInterfaceClass == DEVICE_CLASS_CODE) {
                        itf_desc = interface_list[j].altsetting[k];
                        libusb_free_config_descriptor(config_desc);    
                        return;
                    }
                }
            }
            libusb_free_config_descriptor(config_desc);
        }
    }
}

void release_usb() {
    libusb_free_device_list(list, 1);
    libusb_exit(NULL);
}

// Clean up the allocated resources
void cleanup_usb(struct libusb_transfer *transfer,
    struct libusb_device_handle *handle, 
    struct libusb_interface_descriptor itf_desc,
    struct libusb_config_descriptor *config_desc) {

    libusb_free_transfer(transfer);
    libusb_release_interface(handle, itf_desc.bInterfaceNumber);
    libusb_close(handle);
    libusb_free_config_descriptor(config_desc);
    release_usb();
}

// Try to read/write from/to the target device through USB
// Return -1 if there is a error while communicating with the target
int usb_io_communicate(uint8_t buf[], uint16_t len, uint8_t ep_index)
{
    init_usb();
    // Get the active configuration descriptor
    struct libusb_config_descriptor *config_desc;
    if (libusb_get_active_config_descriptor(device, &config_desc) != 0)
    {
        libusb_free_config_descriptor(config_desc);
        release_usb();
        return -1;
    }

    // Open an device and obtain a device handle to perform I/O operations
    struct libusb_device_handle *handle;
    if (libusb_open(device, &handle) != 0)
    {
        libusb_free_config_descriptor(config_desc);
        release_usb();
        return -1;
    }

    // Claim the interface determined by the init() procedure to perform I/O on its endpoints.
    if (libusb_claim_interface(handle, itf_desc.bInterfaceNumber) != 0)
    {
        libusb_close(handle);
        libusb_free_config_descriptor(config_desc);
        release_usb();
        return -1;
    }

    // Get the address of the target endpoint
    uint8_t ep_addr = itf_desc.endpoint[ep_index].bEndpointAddress;
    // Allocate a libusb transfer for bulk transfer
    struct libusb_transfer *transfer = libusb_alloc_transfer(0);
    // Populate transfer fields for a bulk transfer, and
    // the usb_callback function will be called after the transfer completes
    libusb_fill_bulk_transfer(transfer, handle, ep_addr, buf, len, usb_callback, NULL, 0);

    // Submit the transfer
    if (libusb_submit_transfer(transfer) != 0)
    {
        libusb_release_interface(handle, itf_desc.bInterfaceNumber);
        libusb_close(handle);
        libusb_free_config_descriptor(config_desc);
        release_usb();
        return -1;
    }

    // While the callback haven't been called yet
    while (callback_triggered == 0) {
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        // Set a timeout for 10 seconds
        int error_code = libusb_handle_events_timeout(NULL, &tv);
        if (callback_triggered == 0 || error_code < 0)
        {    
            cleanup_usb(transfer, handle, itf_desc, config_desc);
            return -1;
        }
    }

    callback_triggered = 0;
    cleanup_usb(transfer, handle, itf_desc, config_desc);
    return 0;
}

void read_usb(uint8_t buf[], uint16_t len)
{
    while (usb_io_communicate(buf, len, EP_IN_INDEX) != 0);
}

void write_usb(uint8_t buf[], uint16_t len)
{
    while (usb_io_communicate(buf, len, EP_OUT_INDEX) != 0);
}