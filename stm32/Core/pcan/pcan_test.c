/**
 * @file pcan_test.c
 * @brief Test module for sending periodic CAN messages to PC via USB
 *
 * Sends classic CAN frames every second on both CAN channels to verify
 * that the PC can receive CAN messages successfully via USB.
 *
 * NOTE: This directly injects messages into the protocol layer to bypass
 * CAN bus requirements (ACK, termination, etc.)
 */

#include "pcan_test.h"
#include "pcanpro_can.h"
#include "pcanpro_protocol.h"
#include "pcanpro_timestamp.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

// Test state
static uint32_t last_send_time = 0;
static uint32_t message_counter_ch1 = 0;
static uint32_t message_counter_ch2 = 0;
static uint8_t driver_ready = 0;

// Forward declaration of protocol RX frame function
extern int pcan_protocol_rx_frame(uint8_t channel, struct t_can_msg *pmsg);

/**
 * @brief Send a test CAN message directly to protocol layer
 * @param channel CAN channel (0 or 1)
 * @param can_id CAN message ID
 * @param data Pointer to up to 8 bytes of data
 * @param dlc Data length code (0-8)
 */
static void send_test_message(uint8_t channel, uint32_t can_id,
                              const uint8_t *data, uint8_t dlc) {
  struct t_can_msg msg = {0};

  msg.id = can_id;
  msg.size = dlc > 8 ? 8 : dlc;
  msg.flags = 0; // Standard frame, no RTR, no extended
  msg.timestamp = pcan_timestamp_us();
  memcpy(msg.data, data, msg.size);

  // Directly inject into protocol RX handler (bypasses CAN hardware)
  int result = pcan_protocol_rx_frame(channel, &msg);

  if (result == 0) {
    printf("Test: OK CH%d ID=0x%03X\r\n", channel + 1, (unsigned int)can_id);
  } else {
    printf("Test: ERR CH%d=%d\r\n", channel + 1, result);
  }
}

/**
 * @brief Initialize the test module
 */
void pcan_test_init(void) {
  last_send_time = HAL_GetTick();
  message_counter_ch1 = 0;
  message_counter_ch2 = 0;
  driver_ready = 0;

  printf("PCAN Test: Init\r\n");
}

/**
 * @brief Poll function - sends test messages every second
 */
void pcan_test_poll(void) {
  // Wait for PC driver to load before sending test messages
  if (!pcan_protocol_is_driver_loaded()) {
    if (!driver_ready) {
      driver_ready = 1;
      printf("Test: Waiting for PC driver...\r\n");
    }
    return;
  }

  // Announce driver is ready (once)
  if (driver_ready == 1) {
    driver_ready = 2;
    printf("Test: PC driver loaded! Starting test messages\r\n");
  }

  uint32_t current_time = HAL_GetTick();

  // Send messages every 5000 ms (5 seconds)
  if ((current_time - last_send_time) >= 1) {
    last_send_time = current_time;

    // --- Channel 1 Test Message ---
    uint8_t test_data_ch1[8] = {0x11,
                                0x22,
                                0x33,
                                0x44,
                                (message_counter_ch1 >> 24) & 0xFF,
                                (message_counter_ch1 >> 16) & 0xFF,
                                (message_counter_ch1 >> 8) & 0xFF,
                                (message_counter_ch1) & 0xFF};
    send_test_message(0, 0x123, test_data_ch1, 8);
    message_counter_ch1++;

    // --- Channel 2 Test Message ---
    uint8_t test_data_ch2[8] = {0xAA,
                                0xBB,
                                0xCC,
                                0xDD,
                                (message_counter_ch2 >> 24) & 0xFF,
                                (message_counter_ch2 >> 16) & 0xFF,
                                (message_counter_ch2 >> 8) & 0xFF,
                                (message_counter_ch2) & 0xFF};
    send_test_message(1, 0x456, test_data_ch2, 8);
    message_counter_ch2++;

    printf("Test: #%u/%u\r\n", (unsigned int)message_counter_ch1,
           (unsigned int)message_counter_ch2);
  }
}
