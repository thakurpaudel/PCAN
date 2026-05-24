#pragma once
#include <stdint.h>
#include "tusb.h"
#include "pcan_usbpro_fw.h"

#define PCAN_USB_BUFFER_CMD  (0)
#define PCAN_USB_BUFFER_DATA (1)

// PCAN-USB Pro FD Configuration
#define PCAN_SERIAL_NUMBER   0x00003039 // Decimal 12345
#define PCAN_DEVICE_NUMBER   0xFFFFFFFF

void pcan_protocol_init( void );
void pcan_protocol_poll( void );
void pcan_protocol_process_data( uint8_t ep, uint8_t *ptr, uint16_t size );
void pcan_ep0_receive( void );
bool pcan_protocol_device_setup(tusb_control_request_t const * req);

// Helper to check if PC driver is loaded (for test module)
uint8_t pcan_protocol_is_driver_loaded(void);
