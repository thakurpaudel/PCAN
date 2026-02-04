#pragma once
#include "usbd_def.h"
#include <stdint.h>

// External USB device handle
extern USBD_HandleTypeDef usbd_dev;

struct t_m2h_fsm {
  uint8_t state;
  uint8_t ep_addr;
  uint8_t *pdbuf;
  uint32_t dbsize;
  uint32_t total_tx;
};

void pcan_usb_device_init(void);
void pcan_usb_device_poll(void);
uint16_t pcan_usb_frame_number(void);
int pcan_flush_ep(uint8_t ep);
int pcan_flush_data(struct t_m2h_fsm *pfsm, void *src, int size);
void pcan_usb_set_leds_from_bitmask(uint8_t led_mask);
