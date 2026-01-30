/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_2_Pin GPIO_PIN_2
#define LED_2_GPIO_Port GPIOE
#define LED_1_Pin GPIO_PIN_7
#define LED_1_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */
/* LED Mapping for PCAN Firmware */
#define LED_STAT_PORT LED_1_GPIO_Port
#define LED_STAT_PIN LED_1_Pin

// Map all Channel Activity to LED_2 (Shared)
#define LED_CH0_TX_PORT LED_2_GPIO_Port
#define LED_CH0_TX_PIN LED_2_Pin
#define LED_CH0_RX_PORT LED_2_GPIO_Port
#define LED_CH0_RX_PIN LED_2_Pin

#define LED_CH1_TX_PORT LED_2_GPIO_Port
#define LED_CH1_TX_PIN LED_2_Pin
#define LED_CH1_RX_PORT LED_2_GPIO_Port
#define LED_CH1_RX_PIN LED_2_Pin

/* USER CODE END Private defines */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
