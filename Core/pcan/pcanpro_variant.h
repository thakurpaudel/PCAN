#pragma once

// Define your hardware variant here
// Options: DEVEBOX_H743, NUCLEO_H743, CUSTOM_H743, EMPTY_STUB
#define DEVEBOX_H743

#if (defined DEVEBOX_H743)
/* FDCAN pins are configured by CubeMX - see fdcan.c */
/* FDCAN1: PD0 (RX), PD1 (TX) - AF9_FDCAN1 */
/* FDCAN2: PB5 (RX), PB6 (TX) - AF9_FDCAN2 */

/* LED pins - matching CubeMX configuration (gpio.c) */
/* LED_1: PC7, LED_2: PE2 */
/* We'll use these for CAN activity indication */
#define LED_STAT_PORT GPIOC
#define LED_STAT_PIN GPIO_PIN_7 // LED_1 (PC7)

#define LED_CH0_TX_PORT GPIOE
#define LED_CH0_TX_PIN GPIO_PIN_2 // LED_2 (PE2) - shared for CH0 activity

#define LED_CH0_RX_PORT GPIOE
#define LED_CH0_RX_PIN GPIO_PIN_2 // LED_2 (PE2) - shared for CH0 activity

#define LED_CH1_TX_PORT GPIOC
#define LED_CH1_TX_PIN GPIO_PIN_7 // LED_1 (PC7) - shared for CH1 activity

#define LED_CH1_RX_PORT GPIOC
#define LED_CH1_RX_PIN GPIO_PIN_7 // LED_1 (PC7) - shared for CH1 activity

/* LED polarity (active high) */
#define LED_ON GPIO_PIN_SET
#define LED_OFF GPIO_PIN_RESET

/* Hardware initialization - GPIOs are initialized by CubeMX */
#define IO_HW_INIT()

#elif (defined NUCLEO_H743)
/* FDCAN1 pins on NUCLEO-H743ZI (PD0/PD1) */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* FDCAN2 pins on NUCLEO-H743ZI (PB12/PB13) */
#define CAN2_RX B, 12, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2
#define CAN2_TX B, 13, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2

/* Nucleo user LEDs (LD1=Green PB0, LD2=Yellow PB7, LD3=Red PB14) */
#define IO_LED_STAT B, 0, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX0 B, 7, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX0 B, 14, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX1 E, 1, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX1 C, 13, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF

/* LED polarity (active high on Nucleo) */
#define IO_LED_HI PIN_HI
#define IO_LED_LOW PIN_LOW

/* Hardware initialization */
#define IO_HW_INIT()

#elif (defined HEXV2_CLONE_H7)
/* FDCAN1 pins - using AF9_FDCAN1 for STM32H7 */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* LEDs - Red, Blue, Green */
#define IO_LED_STAT                                                            \
  C, 10, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* red */
#define IO_LED_TX0                                                             \
  E, 0, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* blue */
#define IO_LED_RX0                                                             \
  B, 9, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF /* green */

/* LED polarity (inverted for common anode) */
#define IO_LED_HI PIN_LOW
#define IO_LED_LOW PIN_HI

/* CAN transceiver standby control */
#define CAN1_S E, 10, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF

/* Hardware initialization - enable CAN transceiver */
#define IO_HW_INIT()                                                           \
  PORT_ENABLE_CLOCK(PIN_PORT(CAN1_S), PIN_PORT(CAN1_S));                       \
  PIN_LOW(CAN1_S);                                                             \
  PIN_INIT(CAN1_S)

#elif (defined CUSTOM_H743)
/* Define your custom STM32H743 hardware here */

/* FDCAN1 pins - Customize based on your schematic */
#define CAN1_RX D, 0, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1
#define CAN1_TX D, 1, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN1

/* FDCAN2 pins - Customize based on your schematic */
#define CAN2_RX B, 12, MODE_AF_PP, PULLUP, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2
#define CAN2_TX B, 13, MODE_AF_PP, NOPULL, SPEED_FREQ_VERY_HIGH, AF9_FDCAN2

/* LED pins - Customize based on your schematic */
#define IO_LED_STAT E, 3, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX0 E, 4, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX0 E, 5, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_TX1 E, 6, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF
#define IO_LED_RX1 E, 7, MODE_OUTPUT_PP, NOPULL, SPEED_FREQ_MEDIUM, NOAF

/* LED polarity */
#define IO_LED_HI PIN_HI
#define IO_LED_LOW PIN_LOW

/* Optional: CAN transceiver control pins */
// #define CAN1_S        E, 10, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF
// #define CAN2_S        E, 11, MODE_OUTPUT_OD, PULLUP, SPEED_FREQ_MEDIUM, NOAF

/* Hardware initialization */
#define IO_HW_INIT()


#elif (defined EMPTY_STUB)
/* Empty stub for testing without hardware */
#define IO_HW_INIT()

#else
#error                                                                         \
    "Invalid hardware variant! Define one of: DEVEBOX_H743, NUCLEO_H743, HEXV2_CLONE_H7, CUSTOM_H743, EMPTY_STUB"
#endif

/*
 * STM32H743 FDCAN Pin Options Reference:
 *
 * FDCAN1 (AF9):
 * - RX: PD0, PA11, PH14, PI9
 * - TX: PD1, PA12, PH13, PB9
 *
 * FDCAN2 (AF9):
 * - RX: PB5, PB12
 * - TX: PB6, PB13
 *
 * FDCAN3 (AF9) - if available on your package:
 * - RX: PA8, PF6
 * - TX: PA15, PF7
 *
 * Note: Always verify pin availability for your specific package (LQFP100,
 * LQFP144, etc.)
 */