#include <assert.h>
#include "pcanpro_timestamp.h"

#if defined(ESP_PLATFORM)
#include "esp_timer.h"

void pcan_timestamp_init( void )
{
  // ESP32 timer is initialized automatically
}

uint32_t pcan_timestamp_millis( void )
{
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

uint32_t pcan_timestamp_us( void )
{
  return (uint32_t)esp_timer_get_time();
}
#else
#include <stm32h7xx_hal.h>
#include "stm32h7xx_hal_tim.h"
#include "tim.h"

void pcan_timestamp_init( void )
{
  HAL_TIM_Base_Start(&htim2);
}

uint32_t pcan_timestamp_millis( void )
{
  return HAL_GetTick();
}

uint32_t pcan_timestamp_us( void )
{
  return TIM2->CNT;
}
#endif
