/**
 * @file pcan_test.h
 * @brief Test module for sending periodic CAN messages to PC via USB
 *
 * This module sends test CAN messages every second on both channels
 * to verify USB-to-PC communication is working correctly.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Initialize the test module
 */
void pcan_test_init(void);

/**
 * @brief Poll function to be called in main loop
 * Sends test messages periodically
 */
void pcan_test_poll(void);
