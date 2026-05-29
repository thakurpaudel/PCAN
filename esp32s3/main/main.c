#include "device/usbd_pvt.h" // For custom driver
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcanpro_led.h"
#include "pcanpro_protocol.h"
#include "pcanpro_usbd.h"
#include "pcanpro_timestamp.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

#define EP_CMD_OUT 0x01
#define EP_CMD_IN 0x81
#define EP_MSG1_OUT 0x02
#define EP_MSG1_IN 0x82
#define EP_MSG2_OUT 0x03
#define EP_MSG2_IN 0x83

#if PCAN_NUM_CHANNELS > 1
#define PCAN_USB_ENDPOINT_COUNT 6
#else
#define PCAN_USB_ENDPOINT_COUNT 4
#endif

// ESP32-S3 is USB full-speed. PEAK sends full-speed OUT transfers as 64-byte
// chunks, so each queued OUT transfer must be one max-packet chunk.
static uint8_t ep_cmd_out_buf[64] __attribute__((aligned(4)));
static uint8_t ep_msg1_out_buf[64] __attribute__((aligned(4)));
#if PCAN_NUM_CHANNELS > 1
static uint8_t ep_msg2_out_buf[64] __attribute__((aligned(4)));
#endif

// Buffer for IN endpoints (transmit) to prevent corruption while DMA is active
static uint8_t ep_in_buf[4][2048] __attribute__((aligned(4)));

// Implement pcan_usb_transmit
bool pcan_usb_transmit(uint8_t ep, uint8_t *buf, uint16_t size)
{
    if (!tud_ready())
        return false;
    if (usbd_edpt_busy(TUD_OPT_RHPORT, ep))
        return false;

    uint8_t idx = ep & 0x0F;
    if (idx < 4 && size <= sizeof(ep_in_buf[idx]))
    {
        memcpy(ep_in_buf[idx], buf, size);
        return usbd_edpt_xfer(TUD_OPT_RHPORT, ep, ep_in_buf[idx], size);
    }
    return false;
}

int pcan_flush_ep(uint8_t ep)
{
    return 1;
}

// -------------------------------------------------------------------------
// Custom TinyUSB Class Driver for PCAN
// -------------------------------------------------------------------------
static void pcan_init(void)
{
}

static void pcan_reset(uint8_t rhport)
{
}

static uint16_t pcan_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len)
{
    uint16_t drv_len = sizeof(tusb_desc_interface_t) + PCAN_USB_ENDPOINT_COUNT * sizeof(tusb_desc_endpoint_t);
    if (max_len < drv_len)
        return 0;

    // Open PCAN endpoints
    tusb_desc_endpoint_t const *ep_desc = (tusb_desc_endpoint_t const *)(itf_desc + 1);
    for (int i = 0; i < PCAN_USB_ENDPOINT_COUNT; i++)
    {
        usbd_edpt_open(rhport, ep_desc);
        ep_desc++;
    }

    // Prepare RX for OUT endpoints
    usbd_edpt_xfer(rhport, EP_CMD_OUT, ep_cmd_out_buf, sizeof(ep_cmd_out_buf));
    usbd_edpt_xfer(rhport, EP_MSG1_OUT, ep_msg1_out_buf, sizeof(ep_msg1_out_buf));
#if PCAN_NUM_CHANNELS > 1
    usbd_edpt_xfer(rhport, EP_MSG2_OUT, ep_msg2_out_buf, sizeof(ep_msg2_out_buf));
#endif

    return drv_len;
}

static bool pcan_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    if (stage == CONTROL_STAGE_SETUP)
    {
        return pcan_protocol_device_setup(request);
    }
    else if (stage == CONTROL_STAGE_ACK)
    {
        if (request->bRequest == USB_VENDOR_REQUEST_FKT &&
            request->wValue == USB_VENDOR_REQUEST_wVALUE_SETFKT_INTERFACE_DRIVER_LOADED)
        {
            pcan_ep0_receive();
        }
    }
    return true;
}

// Global control transfer callback for vendor requests (recipient device/interface/endpoint)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request)
{
    return pcan_control_xfer_cb(rhport, stage, request);
}

static bool pcan_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes)
{
    if (result != XFER_RESULT_SUCCESS)
        return false;

    if (ep_addr == EP_CMD_OUT)
    {
        pcan_protocol_process_data(ep_addr, ep_cmd_out_buf, xferred_bytes);
        usbd_edpt_xfer(rhport, ep_addr, ep_cmd_out_buf, sizeof(ep_cmd_out_buf));
    }
    else if (ep_addr == EP_MSG1_OUT)
    {
        pcan_protocol_process_data(ep_addr, ep_msg1_out_buf, xferred_bytes);
        usbd_edpt_xfer(rhport, ep_addr, ep_msg1_out_buf, sizeof(ep_msg1_out_buf));
    }
#if PCAN_NUM_CHANNELS > 1
    else if (ep_addr == EP_MSG2_OUT)
    {
        pcan_protocol_process_data(ep_addr, ep_msg2_out_buf, xferred_bytes);
        usbd_edpt_xfer(rhport, ep_addr, ep_msg2_out_buf, sizeof(ep_msg2_out_buf));
    }
#endif
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
    .sof = NULL};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count)
{
    *driver_count = 1;
    return &_pcan_driver;
}

extern void pcan_ep0_receive(void);

bool tud_control_complete_cb(uint8_t rhport, tusb_control_request_t const *request)
{
    (void)rhport;
    if (request->bmRequestType == (TUSB_DIR_OUT | TUSB_REQ_TYPE_VENDOR | TUSB_REQ_RCPT_DEVICE))
    {
        if (request->bRequest == 2)
        { // USB_VENDOR_REQUEST_FKT
            if (request->wValue == 5)
            { // USB_VENDOR_REQUEST_wVALUE_SETFKT_INTERFACE_DRIVER_LOADED
                pcan_ep0_receive();
                return true;
            }
        }
    }
    return true;
}

// Force linker to include usb_descriptors.c.obj
extern uint8_t const *tud_descriptor_device_cb(void);
extern uint8_t const *tud_descriptor_configuration_cb(uint8_t index);
extern uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid);

void dummy_force_descriptors(void)
{
    printf("USB descriptors: %p %p %p\n",
           (void *)tud_descriptor_device_cb,
           (void *)tud_descriptor_configuration_cb,
           (void *)tud_descriptor_string_cb);
}

// -------------------------------------------------------------------------
// Main
// -------------------------------------------------------------------------

void app_main(void)
{
    printf("Starting PCAN ESP32-S3 Firmware...\n");
    dummy_force_descriptors();

    printf("Initializing USB PHY...\n");
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
        .otg_speed = USB_PHY_SPEED_UNDEFINED,
        .ext_io_conf = NULL,
        .otg_io_conf = NULL,
    };
    usb_phy_handle_t phy_hdl;
    ESP_ERROR_CHECK(usb_new_phy(&phy_conf, &phy_hdl));

    pcan_timestamp_init();
    pcan_led_init();
    pcan_led_set_mode(LED_STAT, LED_MODE_BLINK_FAST, 0xFFFFFFFF);

    // Init PCAN Protocol
    pcan_protocol_init();

    // Init TinyUSB
    tusb_init();

    static uint32_t loop_cnt = 0;
    while (1)
    {
        tud_task_ext(0, false);
        pcan_protocol_poll();
        pcan_led_poll();
        tud_task_ext(0, false);

        loop_cnt++;
        if ((loop_cnt & 0x3FFu) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
