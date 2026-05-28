#include "pcanpro_can.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

/* ESP32-S3 has only one TWAI peripheral — override CAN_BUS_TOTAL */
#ifndef CAN_BUS_TOTAL
#define CAN_BUS_TOTAL 1
#endif
#define TX_GPIO_NUM CONFIG_PCAN_TX_PIN
#define RX_GPIO_NUM CONFIG_PCAN_RX_PIN

#define CAN_TX_FIFO_SIZE (512)

static struct t_can_dev {
  uint32_t tx_msgs;
  uint32_t tx_errs;
  uint32_t tx_ovfs;
  uint32_t rx_msgs;
  uint32_t rx_errs;
  uint32_t rx_ovfs;

  struct t_can_msg tx_fifo[CAN_TX_FIFO_SIZE];
  uint32_t tx_head;
  uint32_t tx_tail;

  int (*rx_isr)(uint8_t, struct t_can_msg *);
  int (*tx_isr)(uint8_t, struct t_can_msg *);
  void (*err_handler)(int bus, uint32_t esr);

  bool is_installed;
} can_dev_array[CAN_BUS_TOTAL];

static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);

extern int pcan_protocol_can_rx_ready(void);

uint32_t pcan_can_msg_time(const struct t_can_msg *pmsg, uint32_t nt, uint32_t dt) {
    const uint32_t data_bits = pmsg->size << 3;
    const uint32_t control_bits = (pmsg->flags & MSG_FLAG_EXT) ? 67 : 47;
    return (control_bits + data_bits) * nt;
}

int pcan_can_stats(int bus, struct t_can_stats *p_stats) {
    if (bus >= CAN_BUS_TOTAL || !p_stats) return -1;
    p_stats->tx_msgs = can_dev_array[bus].tx_msgs;
    p_stats->tx_errs = can_dev_array[bus].tx_errs;
    p_stats->rx_msgs = can_dev_array[bus].rx_msgs;
    p_stats->rx_errs = can_dev_array[bus].rx_errs;
    p_stats->rx_ovfs = can_dev_array[bus].rx_ovfs;
    return 0;
}

static twai_timing_config_t get_twai_timing(uint32_t bitrate) {
    switch (bitrate) {
        case 1000000: return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
        case 800000:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_800KBITS();
        case 500000:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
        case 250000:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
        case 125000:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
        case 100000:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_100KBITS();
        case 50000:   return (twai_timing_config_t)TWAI_TIMING_CONFIG_50KBITS();
        case 25000:   return (twai_timing_config_t)TWAI_TIMING_CONFIG_25KBITS();
        default:      return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    }
}

int pcan_can_init_ex(int bus, uint32_t bitrate) {
    if (bus != CAN_BUS_1) return 0; // ESP32-S3 only has one TWAI
    
    static uint32_t current_bitrate = 0;
    if (can_dev_array[bus].is_installed && current_bitrate == bitrate) {
        printf("TWAI CAN already initialized at %lu bps\n", (unsigned long)bitrate);
        return 0;
    }
    
    if (can_dev_array[bus].is_installed) {
        twai_stop();
        twai_driver_uninstall();
        can_dev_array[bus].is_installed = false;
    }

    current_bitrate = bitrate;
    t_config = get_twai_timing(bitrate);
    g_config.tx_queue_len = 256;
    g_config.rx_queue_len = 1024;

    printf("TWAI CAN configured for %lu bps (deferred install)\n", (unsigned long)bitrate);
    return 0;
}

int pcan_can_init(int bus, uint32_t bitrate) {
    return pcan_can_init_ex(bus, bitrate);
}

int pcan_can_init_fd(int bus, uint32_t nom_bitrate, uint32_t data_bitrate) {
    // ESP32-S3 does not support CAN-FD, fallback to classic
    printf("WARNING: CAN-FD requested but ESP32-S3 only supports Classic CAN\n");
    return pcan_can_init_ex(bus, nom_bitrate);
}

void pcan_can_set_silent(int bus, uint8_t silent_mode) {
    if (bus != CAN_BUS_1) return;
    printf("Setting SILENT MODE: %d\n", silent_mode);
    g_config.mode = silent_mode ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL;
    // Need to re-init for mode change
    if (can_dev_array[bus].is_installed) {
        twai_stop();
        twai_driver_uninstall();
        twai_driver_install(&g_config, &t_config, &f_config);
        twai_start();
    }
}

void pcan_can_set_iso_mode(int bus, uint8_t iso_mode) { /* Not supported */ }
void pcan_can_set_loopback(int bus, uint8_t loopback) { /* Not supported without re-init */ }
void pcan_can_set_bus_active(int bus, uint16_t mode) {
    if (bus != CAN_BUS_1) return;
    struct t_can_dev *p_dev = &can_dev_array[bus];

    if (mode == 0) {
        // RESET_MODE: Stop and uninstall TWAI to stop ACKing frames.
        // This pauses the OBD tool's boot sequence.
        if (p_dev->is_installed) {
            twai_stop();
            twai_driver_uninstall();
            p_dev->is_installed = false;
            printf("TWAI stopped and uninstalled (RESET_MODE)\n");
        }
    } else {
        // NORMAL_MODE: Install and start the TWAI driver.
        if (!p_dev->is_installed) {
            printf("TWAI Install config: brp=%lu, tseg1=%d, tseg2=%d, sjw=%d\n", (unsigned long)t_config.brp, t_config.tseg_1, t_config.tseg_2, t_config.sjw);
            if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
                if (twai_start() == ESP_OK) {
                    p_dev->is_installed = true;
                    printf("TWAI installed and started (NORMAL_MODE)\n");
                } else {
                    printf("TWAI start FAILED!\n");
                }
            } else {
                printf("TWAI install FAILED!\n");
            }
        } else {
            printf("TWAI already active (NORMAL_MODE)\n");
        }
    }
}

void pcan_can_set_bitrate(int bus, uint32_t bitrate, int is_data_bitrate) {
    if (is_data_bitrate) return; // Ignore data bitrate
    pcan_can_init_ex(bus, bitrate);
}

void pcan_can_set_bitrate_ex(int bus, uint16_t brp, uint8_t tseg1, uint8_t tseg2, uint8_t sjw) {
    // Ignore raw CAN-FD timing parameters from the host PC.
    // SJA1000 controller (ESP32 TWAI) cannot support them.
    // Instead, we rely entirely on standard mapping in pcan_can_init_ex().
    printf("Ignoring direct BTR overwrite from PC: brp=%d, tseg1=%d, tseg2=%d\n", brp, tseg1, tseg2);
}

void pcan_can_set_bitrate_raw(int bus, int bit_rate, int is_data) {
    pcan_can_set_bitrate(bus, bit_rate, is_data);
}

int pcan_can_write(int bus, struct t_can_msg *p_msg) {
    if (bus != CAN_BUS_1 || !p_msg) return 0;
    
    struct t_can_dev *p_dev = &can_dev_array[bus];

    if (p_msg->size > 8) {
        p_dev->tx_errs++;
        return -1;
    }

    if (p_msg->flags & MSG_FLAG_EXT) {
        if (p_msg->id > 0x1FFFFFFF) {
            p_dev->tx_errs++;
            return -1;
        }
    } else if (p_msg->id > 0x7FF) {
        p_dev->tx_errs++;
        return -1;
    }

    uint32_t tx_head_next = (p_dev->tx_head + 1) & (CAN_TX_FIFO_SIZE - 1);
    
    if (tx_head_next == p_dev->tx_tail) {
        p_dev->tx_ovfs++;
        p_dev->tx_errs++;
        return -1;
    }
    
    p_dev->tx_fifo[p_dev->tx_head] = *p_msg;
    p_dev->tx_head = tx_head_next;
    return 0;
}

void pcan_can_install_rx_callback(int bus, int (*rx_isr)(uint8_t, struct t_can_msg *)) {
    if (bus < CAN_BUS_TOTAL) can_dev_array[bus].rx_isr = rx_isr;
}

void pcan_can_install_tx_callback(int bus, int (*tx_isr)(uint8_t, struct t_can_msg *)) {
    if (bus < CAN_BUS_TOTAL) can_dev_array[bus].tx_isr = tx_isr;
}

void pcan_can_install_err_callback(int bus, void (*cb)(int, uint32_t)) {
    if (bus < CAN_BUS_TOTAL) can_dev_array[bus].err_handler = cb;
}

int pcan_can_set_filter_mask(int bus, int num, int format, uint32_t id, uint32_t mask) {
    // Simplification: We use ACCEPT_ALL by default since TWAI only has 1 dual filter
    return 0;
}

int pcan_can_filter_init_stdid_list(int bus, const uint16_t *id_list, int id_len) {
    return 0;
}

static void pcan_can_flush_tx(int bus) {
    if (bus != CAN_BUS_1 || !can_dev_array[bus].is_installed) return;
    struct t_can_dev *p_dev = &can_dev_array[bus];

    while (p_dev->tx_head != p_dev->tx_tail) {
        struct t_can_msg *p_msg = &p_dev->tx_fifo[p_dev->tx_tail];
        twai_message_t t_msg = {0};

        if (p_msg->flags & MSG_FLAG_EXT) {
            t_msg.identifier = p_msg->id & 0x1FFFFFFF;
            t_msg.extd = 1;
        } else {
            t_msg.identifier = p_msg->id & 0x7FF;
            t_msg.extd = 0;
        }
        
        t_msg.rtr = (p_msg->flags & MSG_FLAG_RTR) ? 1 : 0;
        t_msg.data_length_code = p_msg->size > 8 ? 8 : p_msg->size;
        if (!t_msg.rtr) {
            memcpy(t_msg.data, p_msg->data, t_msg.data_length_code);
        }

        if (twai_transmit(&t_msg, 0) == ESP_OK) {
            if (p_dev->tx_isr) {
                p_dev->tx_isr(bus, p_msg);
            }
            p_dev->tx_msgs++;
            p_dev->tx_tail = (p_dev->tx_tail + 1) & (CAN_TX_FIFO_SIZE - 1);
        } else {
            // TX queue full or error, wait for next poll
            p_dev->tx_errs++;
            break;
        }
    }
}

void pcan_can_poll(void) {
    static uint32_t poll_count = 0;
    poll_count++;
    for (int bus = 0; bus < CAN_BUS_TOTAL; bus++) {
        if (!can_dev_array[bus].is_installed) {
            if ((poll_count & 0xFFFFu) == 1)
                printf("TWAI bus%d NOT installed (poll#%lu)\n", bus, (unsigned long)poll_count);
            continue;
        }
        
        if ((poll_count & 0xFFFu) == 0) {
            twai_status_info_t status;
            static uint32_t last_tx_failed[CAN_BUS_TOTAL];
            static uint32_t last_rx_missed[CAN_BUS_TOTAL];
            twai_get_status_info(&status);
            can_dev_array[bus].tx_errs += status.tx_failed_count - last_tx_failed[bus];
            can_dev_array[bus].rx_ovfs += status.rx_missed_count - last_rx_missed[bus];
            last_tx_failed[bus] = status.tx_failed_count;
            last_rx_missed[bus] = status.rx_missed_count;
        }

        twai_message_t t_msg;
        while (pcan_protocol_can_rx_ready() &&
               twai_receive(&t_msg, 0) == ESP_OK) {
            struct t_can_msg p_msg = {0};
            p_msg.id = t_msg.identifier;
            p_msg.flags = 0;
            if (t_msg.extd) p_msg.flags |= MSG_FLAG_EXT;
            if (t_msg.rtr)  p_msg.flags |= MSG_FLAG_RTR;
            p_msg.size = t_msg.data_length_code;
            p_msg.timestamp = (uint32_t)esp_timer_get_time();

            if (!t_msg.rtr)
                memcpy(p_msg.data, t_msg.data, t_msg.data_length_code);

            can_dev_array[bus].rx_msgs++;

            if (can_dev_array[bus].rx_isr && can_dev_array[bus].rx_isr(bus, &p_msg) < 0)
                can_dev_array[bus].rx_ovfs++;

        }
        
        // 2. Process TX
        pcan_can_flush_tx(bus);
    }
}
