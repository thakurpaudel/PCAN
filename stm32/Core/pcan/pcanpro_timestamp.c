#include <assert.h>
#include <stm32h7xx_hal.h>
#include "pcanpro_timestamp.h"
#include "stm32h7xx_hal_tim.h"
#include "tim.h"


void pcan_timestamp_init( void )
{
  HAL_TIM_Base_Start(&htim2);
}

uint32_t pcan_timestamp_millis( void )
{
  return  HAL_GetTick();
}

uint32_t pcan_timestamp_us( void )
{
  uint32_t tim_lo;

  tim_lo = TIM2->CNT;

  return (tim_lo);
}
