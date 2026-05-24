
#pragma once
#if !defined(ESP_PLATFORM)
#include "usbd_def.h"
#endif
#include <stdint.h>

/* USB Packet Sizes */
#define PCAN_FS_MAX_BULK_PACKET_SIZE (64)
#define PCAN_HS_MAX_BULK_PACKET_SIZE (512)

/* USB Vendor/Product IDs */
#define PCAN_USB_VENDOR_ID 0x0C72       // PEAK-System official VID
#define PCAN_USBPROFD_PRODUCT_ID 0x0011 // PCAN-USB Pro FD
#define PCAN_USBPRO_PRODUCT_ID 0x000D   // PCAN-USB Pro
#define PCAN_USBFD_PRODUCT_ID 0x0012    // PCAN-USB FD

/* PCAN-USB Endpoints */
#define PCAN_USB_EP_CMDOUT 0x01
#define PCAN_USB_EP_CMDIN 0x81
#define PCAN_USB_EP_MSGOUT_CH1 0x02
#define PCAN_USB_EP_MSGIN_CH1 0x82
#define PCAN_USB_EP_MSGOUT_CH2 0x03
#define PCAN_USB_EP_MSGIN_CH2 0x83

/* Board-specific packet sizes based on build configuration */
/* Classic CAN Mode - PCAN_PRO or PCAN_CLASSIC_MODE */
#if defined(PCAN_PRO) || defined(PCAN_CLASSIC_MODE)
#define PCAN_DATA_PACKET_SIZE (64)
#define PCAN_CMD_PACKET_SIZE (512)
#define PCAN_SUPPORTS_FD (0)
#define PCAN_PRODUCT_ID PCAN_USBPRO_PRODUCT_ID
#define PCAN_PRODUCT_STRING "PCAN-USB Pro"
#define PCAN_NUM_CHANNELS (2)

/* CAN-FD Mode - PCAN_PRO_FD, PCAN_FD, or PCAN_X6 */
#elif defined(PCAN_PRO_FD) || defined(PCAN_FD) || defined(PCAN_X6)
#define PCAN_DATA_PACKET_SIZE (256)
#define PCAN_CMD_PACKET_SIZE (128)
#define PCAN_SUPPORTS_FD (1)

#if defined(PCAN_PRO_FD)
#define PCAN_PRODUCT_ID PCAN_USBPROFD_PRODUCT_ID
#define PCAN_PRODUCT_STRING "PCAN-USB Pro FD"
#define PCAN_NUM_CHANNELS (2)
#elif defined(PCAN_FD)
#define PCAN_PRODUCT_ID PCAN_USBFD_PRODUCT_ID
#define PCAN_PRODUCT_STRING "PCAN-USB FD"
#define PCAN_NUM_CHANNELS (1)
#elif defined(PCAN_X6)
#define PCAN_PRODUCT_ID 0x0014 // PCAN-USB X6
#define PCAN_PRODUCT_STRING "PCAN-USB X6"
#define PCAN_NUM_CHANNELS (2)
#endif

#else
#error                                                                         \
    "No board type defined! Define one of: PCAN_PRO, PCAN_PRO_FD, PCAN_FD, PCAN_X6"
#endif

/* LIN Interface support (optional) */
#ifndef INCLUDE_LIN_INTERFACE
#define INCLUDE_LIN_INTERFACE (0)
#endif

/* Class-specific data structure */
struct t_class_data {
  int tx_flow_control_in_use;
  uint8_t ep_tx_in_use[15];
  uint8_t cmd_ep_buffer[PCAN_CMD_PACKET_SIZE];
  uint8_t data1_ep_buffer[PCAN_DATA_PACKET_SIZE];
  uint8_t data2_ep_buffer[PCAN_DATA_PACKET_SIZE];
};

/* USB Class definition */
#if !defined(ESP_PLATFORM)
extern USBD_ClassTypeDef usbd_pcanpro;
#endif
#define STRINGIFY(x) #x
/* Configuration summary for debugging */
#if defined(DEBUG) && DEBUG == 1
#pragma message "PCAN Configuration:"
#pragma message "  Product: " PCAN_PRODUCT_STRING
#pragma message "  Channels: " STRINGIFY(PCAN_NUM_CHANNELS)
#pragma message "  FD Support: " STRINGIFY(PCAN_SUPPORTS_FD)
#pragma message "  Data Packet Size: " STRINGIFY(PCAN_DATA_PACKET_SIZE)
#pragma message "  CMD Packet Size: " STRINGIFY(PCAN_CMD_PACKET_SIZE)
#pragma message "  LIN Support: " STRINGIFY(INCLUDE_LIN_INTERFACE)
#endif
