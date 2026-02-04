#include "pcan_usb.h"
#include "pcanpro_led.h"
#include "pcanpro_usbd.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h> // For memcpy

extern USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef FS_Desc;
// extern PCD_HandleTypeDef hpcd_usb; // Using default CubeMX handle if
// available
extern void Error_Handler(void);

// Export usbd_dev for test module compatibility (points to hUsbDeviceFS)
USBD_HandleTypeDef usbd_dev;

#ifndef USB_MODULE_ID
#define USB_MODULE_ID DEVICE_FS
#endif

void pcan_usb_device_init(void) {
  // Initialize usbd_dev reference for test module
  usbd_dev = hUsbDeviceFS;

  /* Init Device Library, add supported class and start the library. */
  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, USB_MODULE_ID) != USBD_OK) {
    Error_Handler();
  }
  if (USBD_RegisterClass(&hUsbDeviceFS, &usbd_pcanpro) != USBD_OK) {
    Error_Handler();
  }

  /* Note: USBD_Stop/Start sequence typically used to force re-enumeration */
  if (USBD_Stop(&hUsbDeviceFS) != USBD_OK) {
    Error_Handler();
  }

  HAL_Delay(1000);

  if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
    Error_Handler();
  }

  // Update after Start since USBD_Init may modify the structure
  usbd_dev = hUsbDeviceFS;
}

// Polling mode - manually call IRQ handler
void pcan_usb_device_poll(void) {
  // Call HAL IRQ handler for USB events
  HAL_PCD_IRQHandler((PCD_HandleTypeDef *)hUsbDeviceFS.pData);

}

uint16_t pcan_usb_frame_number(void) {
  uint32_t USBx_BASE =(uint32_t)(((PCD_HandleTypeDef *)hUsbDeviceFS.pData)->Instance);

  return (USBx_DEVICE->DSTS >> 8u) & 0x3FFFu;
}

int pcan_flush_ep(uint8_t ep) {
  USBD_HandleTypeDef *pdev = &hUsbDeviceFS;
  struct t_class_data *p_data = (void *)pdev->pClassData;
  printf("called pcan_flush_ep(%d, 0x%02X)\r\n",ep,ep);
  p_data->ep_tx_in_use[ep & 0x0F] = 0;
  return USBD_LL_FlushEP(pdev, ep) == USBD_OK;
}

int pcan_flush_data(struct t_m2h_fsm *pfsm, void *src, int size) {
  USBD_HandleTypeDef *pdev = &hUsbDeviceFS;
  struct t_class_data *p_data = (void *)pdev->pClassData;

  if (!p_data) {
    printf("USB ERR: p_data is NULL\r\n");
    return 0;
  }
  //printf("calling end poinst are %d, 0x%02X\r\n",  pfsm->ep_addr,  pfsm->ep_addr);
 

  switch (pfsm->state) {
  case 1:
    if (p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F]) {
      // Still busy
      //printf("USB BUSY: EP=0x%02X\r\n", pfsm->ep_addr);
      return 0;
    }
    pfsm->state = 0;
    /* fall through */
  case 0:
    assert(p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F] == 0);
    // size = (size+(64-1))&(~(64-1));
    if (size > pfsm->dbsize) {
      printf("USB ERR: Size too large %d > %ld\r\n", size, pfsm->dbsize);
      break;
    }
    memcpy(pfsm->pdbuf, src, size);
    p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F] = 1;
    /* prepare data transmit */
    pdev->ep_in[pfsm->ep_addr & EP_ADDR_MSK].total_length = size;
    if (USBD_LL_Transmit(pdev, pfsm->ep_addr, pfsm->pdbuf, size) != USBD_OK) {
      printf("USB TX ERR: Transmit failed EP=0x%02X\r\n", pfsm->ep_addr);
      p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F] = 0;
      return 0;
    }

    pfsm->total_tx += size;
    pfsm->state = 1;
    return 1;
  }

  return 0;
}

/**
 * @brief Sets the state of LEDs based on a 4-bit mask.
 *        Bit 0 controls LED_CH0_RX
 *        Bit 1 controls LED_CH1_RX
 *        Bit 2 controls LED_CH0_TX
 *        Bit 3 controls LED_CH1_TX
 * @param led_mask A 4-bit value where each bit corresponds to an LED.
 */
void pcan_usb_set_leds_from_bitmask(uint8_t led_mask) {
  // Array to map bit position to e_pcan_led enum values
  // Assuming this mapping based on common practice and enum definition
  int led_map[] = {LED_CH0_RX, LED_CH1_RX, LED_CH0_TX, LED_CH1_TX};
  const int num_leds_to_control = sizeof(led_map) / sizeof(led_map[0]);

  for (int i = 0; i < num_leds_to_control; i++) {
    if ((led_mask >> i) & 0x01) {
      // Bit is set, turn LED ON
      pcan_led_set_mode(led_map[i], LED_MODE_ON, 0);
    } else {
      // Bit is not set, turn LED OFF
      pcan_led_set_mode(led_map[i], LED_MODE_OFF, 0);
    }
  }
}
