#include "tusb.h"
#include "pcanpro_usbd.h"

#if PCAN_NUM_CHANNELS > 1
#define PCAN_USB_ENDPOINT_COUNT 6
#else
#define PCAN_USB_ENDPOINT_COUNT 4
#endif

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = CONFIG_PCAN_USB_VID,
    .idProduct          = CONFIG_PCAN_USB_PID,
    .bcdDevice          = PCAN_USB_BCD_DEVICE,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03, // Expose serial number to host

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + 9 + PCAN_USB_ENDPOINT_COUNT * 7)

#define EP_CMD_OUT        0x01
#define EP_CMD_IN         0x81
#define EP_MSG1_OUT       0x02
#define EP_MSG1_IN        0x82
#define EP_MSG2_OUT       0x03
#define EP_MSG2_IN        0x83

// TUD_VENDOR_DESCRIPTOR does not support 6 endpoints out of the box, we build it manually
uint8_t const desc_configuration[] =
{
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 4, CONFIG_TOTAL_LEN, 0x80, 240),

    // Interface Descriptor
    9, TUSB_DESC_INTERFACE, 0, 0, PCAN_USB_ENDPOINT_COUNT, 0x00, 0x00, 0x00, 5,

    // Endpoint Descriptor CMD IN
    7, TUSB_DESC_ENDPOINT, EP_CMD_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    // Endpoint Descriptor CMD OUT
    7, TUSB_DESC_ENDPOINT, EP_CMD_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

    // Endpoint Descriptor MSG1 IN
    7, TUSB_DESC_ENDPOINT, EP_MSG1_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    // Endpoint Descriptor MSG1 OUT
    7, TUSB_DESC_ENDPOINT, EP_MSG1_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,

#if PCAN_NUM_CHANNELS > 1
    // Endpoint Descriptor MSG2 IN
    7, TUSB_DESC_ENDPOINT, EP_MSG2_IN, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0,
    // Endpoint Descriptor MSG2 OUT
    7, TUSB_DESC_ENDPOINT, EP_MSG2_OUT, TUSB_XFER_BULK, U16_TO_U8S_LE(64), 0
#endif
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; 
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  CONFIG_PCAN_MANUFACTURER,      // 1: Manufacturer
  CONFIG_PCAN_DEVICE_NAME,       // 2: Product
  CONFIG_PCAN_SERIAL_NUMBER,     // 3: Serial
  "Config00",                    // 4: Configuration
  "PCAN-USB FD CAN"              // 5: Interface string
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;
  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }
  else
  {
    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];
    if (index == 3 && (str == NULL || str[0] == '\0'))
    {
      str = "20540529"; // Fallback to user's working serial number if empty
    }

    chr_count = strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
  return _desc_str;
}

//--------------------------------------------------------------------+
// Device Qualifier Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
    .bLength            = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved          = 0x00
};

uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}
