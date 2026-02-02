#include "pcan_usb.h"
#include "pcanpro_usbd.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include <assert.h>

extern USBD_HandleTypeDef hUsbDeviceFS;
extern USBD_DescriptorsTypeDef FS_Desc;
// extern PCD_HandleTypeDef hpcd_usb; // Using default CubeMX handle if
// available
extern void Error_Handler(void);

#ifndef USB_MODULE_ID
#define USB_MODULE_ID DEVICE_FS
#endif

void pcan_usb_device_init(void) {
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
}

// Polling usually logic for non-interrupt mode, but we use ISR
void pcan_usb_device_poll(void) {
  // HAL_PCD_IRQHandler(&hpcd_usb);
  // Manual Polling Mode Enabled
  HAL_PCD_IRQHandler((PCD_HandleTypeDef *)hUsbDeviceFS.pData);
}

uint16_t pcan_usb_frame_number(void) {
  uint32_t USBx_BASE =
      (uint32_t)(((PCD_HandleTypeDef *)hUsbDeviceFS.pData)->Instance);

  return (USBx_DEVICE->DSTS >> 8u) & 0x3FFFu;
}

int pcan_flush_ep(uint8_t ep) {
  USBD_HandleTypeDef *pdev = &hUsbDeviceFS;
  struct t_class_data *p_data = (void *)pdev->pClassData;

  p_data->ep_tx_in_use[ep & 0x0F] = 0;
  return USBD_LL_FlushEP(pdev, ep) == USBD_OK;
}

int pcan_flush_data(struct t_m2h_fsm *pfsm, void *src, int size) {
  USBD_HandleTypeDef *pdev = &hUsbDeviceFS;
  struct t_class_data *p_data = (void *)pdev->pClassData;

  if (!p_data)
    return 0;

  switch (pfsm->state) {
  case 1:
    if (p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F])
      return 0;
    pfsm->state = 0;
    /* fall through */
  case 0:
    assert(p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F] == 0);
    // size = (size+(64-1))&(~(64-1));
    if (size > pfsm->dbsize) {
      printf("PCAN TX Error: Size %d > DBSize %lu\r\n", size, pfsm->dbsize);
      break;
    }
    memcpy(pfsm->pdbuf, src, size);
    p_data->ep_tx_in_use[pfsm->ep_addr & 0x0F] = 1;
    /* prepare data transmit */
    pdev->ep_in[pfsm->ep_addr & EP_ADDR_MSK].total_length = size;

    // printf("PCAN TX Submit: EP=0x%02X Size=%d\r\n", pfsm->ep_addr, size);
    USBD_LL_Transmit(pdev, pfsm->ep_addr, pfsm->pdbuf, size);

    pfsm->total_tx += size;
    pfsm->state = 1;
    return 1;
  }

  return 0;
}
