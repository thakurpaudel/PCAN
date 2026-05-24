#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "pcanpro_protocol.h"
#include "device/usbd_pvt.h" // For custom driver

#define EP_CMD_OUT        0x01
#define EP_CMD_IN         0x81
#define EP_MSG1_OUT       0x02
#define EP_MSG1_IN        0x82
#define EP_MSG2_OUT       0x03
#define EP_MSG2_IN        0x83

// Buffer for OUT endpoints
static uint8_t ep_out_buf[4][64];

// Implement pcan_usb_transmit
bool pcan_usb_transmit(uint8_t ep, uint8_t *buf, uint16_t size) {
    if (tud_ready()) {
        return usbd_edpt_xfer(TUD_OPT_RHPORT, ep, buf, size);
    }
    return false;
}

int pcan_flush_ep(uint8_t ep) {
    printf("called pcan_flush_ep(0x%02X)\n", ep);
    return 1;
}

// -------------------------------------------------------------------------
// Custom TinyUSB Class Driver for PCAN
// -------------------------------------------------------------------------
static void pcan_init(void) {
}

static void pcan_reset(uint8_t rhport) {
}

static uint16_t pcan_open(uint8_t rhport, tusb_desc_interface_t const * itf_desc, uint16_t max_len) {
    uint16_t drv_len = sizeof(tusb_desc_interface_t) + 6 * sizeof(tusb_desc_endpoint_t);
    if (max_len < drv_len) return 0;
    
    // Open all 6 endpoints
    tusb_desc_endpoint_t const * ep_desc = (tusb_desc_endpoint_t const *) (itf_desc + 1);
    for(int i=0; i<6; i++) {
        usbd_edpt_open(rhport, ep_desc);
        ep_desc++;
    }

    // Prepare RX for OUT endpoints
    usbd_edpt_xfer(rhport, EP_CMD_OUT, ep_out_buf[1], 64);
    usbd_edpt_xfer(rhport, EP_MSG1_OUT, ep_out_buf[2], 64);
    usbd_edpt_xfer(rhport, EP_MSG2_OUT, ep_out_buf[3], 64);

    return drv_len;
}

static bool pcan_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage == CONTROL_STAGE_SETUP) {
        return pcan_protocol_device_setup(request);
    }
    return true;
}

static bool pcan_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    if (result != XFER_RESULT_SUCCESS) return false;

    if (ep_addr == EP_CMD_OUT || ep_addr == EP_MSG1_OUT || ep_addr == EP_MSG2_OUT) {
        uint8_t idx = ep_addr & 0x0F;
        pcan_protocol_process_data(ep_addr, ep_out_buf[idx], xferred_bytes);
        // Queue next receive
        usbd_edpt_xfer(rhport, ep_addr, ep_out_buf[idx], 64);
    }
    return true;
}

static usbd_class_driver_t const _pcan_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "PCAN",
#endif
    .init = pcan_init,
    .reset = pcan_reset,
    .open = pcan_open,
    .control_xfer_cb = pcan_control_xfer_cb,
    .xfer_cb = pcan_xfer_cb,
    .sof = NULL
};

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &_pcan_driver;
}

// Force linker to include usb_descriptors.c.obj
extern uint8_t const * tud_descriptor_device_cb(void);
extern uint8_t const * tud_descriptor_configuration_cb(uint8_t index);
extern uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid);

void dummy_force_descriptors(void) {
    printf("USB descriptors: %p %p %p\n",
           (void*)tud_descriptor_device_cb,
           (void*)tud_descriptor_configuration_cb,
           (void*)tud_descriptor_string_cb);
}

// -------------------------------------------------------------------------
// Main
// -------------------------------------------------------------------------

void app_main(void) {
    printf("Starting PCAN ESP32-S3 Firmware...\n");
    dummy_force_descriptors();
    
    // Init PCAN Protocol
    pcan_protocol_init();
    
    // Init TinyUSB
    tusb_init();

    while (1) {
        tud_task();
        pcan_protocol_poll();
        vTaskDelay(1);
    }
}
