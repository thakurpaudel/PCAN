#pragma once

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+

#ifndef CFG_TUSB_MCU
  #define CFG_TUSB_MCU            OPT_MCU_ESP32S3
#endif

#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE

#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS             OPT_OS_FREERTOS
#endif

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+

#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE  64
#endif

// Since we use our own custom driver, we do not need to enable HID, CDC, MSC, etc.
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

#ifdef __cplusplus
 }
#endif
