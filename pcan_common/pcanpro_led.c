#include "pcanpro_led.h"

#if defined(ESP_PLATFORM)

#include "driver/gpio.h"
#include "pcanpro_timestamp.h"
#include "sdkconfig.h"
#include <assert.h>
#include <stdbool.h>

static struct {
  uint32_t arg;
  uint32_t delay;
  uint32_t timestamp;
  uint16_t mode;
  uint8_t state;
} led_mode_array[LED_TOTAL] = {0};

static bool _led_pin_is_enabled(gpio_num_t pin) {
  return pin >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(pin);
}

static void _led_write_pin(gpio_num_t pin, uint8_t state) {
  if (!_led_pin_is_enabled(pin)) {
    return;
  }

#if CONFIG_PCAN_LED_ACTIVE_LOW
  gpio_set_level(pin, state ? 0 : 1);
#else
  gpio_set_level(pin, state ? 1 : 0);
#endif
}

static gpio_num_t _led_gpio_for_id(int led) {
  switch (led) {
  case LED_STAT:
    return (gpio_num_t)CONFIG_PCAN_USB_LINK_LED_PIN;

  case LED_CH0_RX:
  case LED_CH0_TX:
  case LED_CH1_RX:
  case LED_CH1_TX:
    return (gpio_num_t)CONFIG_PCAN_CAN_ACTIVITY_LED_PIN;

  default:
    return GPIO_NUM_NC;
  }
}

void pcan_led_init(void) {
  gpio_num_t pins[] = {
      (gpio_num_t)CONFIG_PCAN_USB_LINK_LED_PIN,
      (gpio_num_t)CONFIG_PCAN_CAN_ACTIVITY_LED_PIN,
  };

  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    if (!_led_pin_is_enabled(pins[i])) {
      continue;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << pins[i],
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    _led_write_pin(pins[i], 0);
  }
}

void pcan_led_set_mode(int led, int mode, uint32_t arg) {
  assert(led < LED_TOTAL);

  led_mode_array[led].mode = mode;
  if (!led_mode_array[led].timestamp) {
    led_mode_array[led].timestamp = pcan_timestamp_millis();
  }
  led_mode_array[led].delay = 0;

  if (mode == LED_MODE_BLINK_FAST || mode == LED_MODE_BLINK_SLOW) {
    led_mode_array[led].delay = (mode == LED_MODE_BLINK_FAST) ? 50 : 200;
    if (arg != 0xFFFFFFFF) {
      arg = pcan_timestamp_millis() + arg;
    }
  }

  led_mode_array[led].arg = arg;
}

static void _led_update_state(int led, uint8_t state) {
  _led_write_pin(_led_gpio_for_id(led), state);
}

void pcan_led_poll(void) {
  uint32_t ts_ms = pcan_timestamp_millis();

  for (int i = 0; i < LED_TOTAL; i++) {
    if (!led_mode_array[i].timestamp) {
      continue;
    }
    if ((uint32_t)(ts_ms - led_mode_array[i].timestamp) <
        led_mode_array[i].delay) {
      continue;
    }

    switch (led_mode_array[i].mode) {
    default:
    case LED_MODE_DEVICE:
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_OFF:
    case LED_MODE_ON:
      led_mode_array[i].state = (led_mode_array[i].mode == LED_MODE_ON);
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_BLINK_FAST:
    case LED_MODE_BLINK_SLOW:
      led_mode_array[i].state ^= 1;
      led_mode_array[i].timestamp += led_mode_array[i].delay;
      if ((led_mode_array[i].arg != 0xFFFFFFFF) &&
          (led_mode_array[i].arg <= ts_ms)) {
        pcan_led_set_mode(i, LED_MODE_OFF, 0);
      }
      break;
    }

    _led_update_state(i, led_mode_array[i].state);
  }
}

#else

#include "gpio.h"
#include "main.h"
#include "pcanpro_timestamp.h"
#include "pcanpro_variant.h"
#include <assert.h>
static struct {
  uint32_t arg;
  uint32_t delay;
  uint32_t timestamp;
  uint16_t mode;
  uint8_t state;
} led_mode_array[LED_TOTAL] = {0};

void pcan_led_init(void) {
  // GPIO initialization is handled by CubeMX in MX_GPIO_Init()
  // Just ensure LEDs are in known state
  IO_HW_INIT();

  // Set all LEDs to OFF state
  HAL_GPIO_WritePin(LED_STAT_PORT, LED_STAT_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH0_TX_PORT, LED_CH0_TX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH0_RX_PORT, LED_CH0_RX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH1_TX_PORT, LED_CH1_TX_PIN, LED_OFF);
  HAL_GPIO_WritePin(LED_CH1_RX_PORT, LED_CH1_RX_PIN, LED_OFF);
}

void pcan_led_set_mode(int led, int mode, uint32_t arg) {
  assert(led < LED_TOTAL);

  led_mode_array[led].mode = mode;
  if (!led_mode_array[led].timestamp) {
    led_mode_array[led].timestamp = pcan_timestamp_millis();
  }
  led_mode_array[led].delay = 0;

  /* set guard time */
  if (mode == LED_MODE_BLINK_FAST || mode == LED_MODE_BLINK_SLOW) {
    led_mode_array[led].delay = (mode == LED_MODE_BLINK_FAST) ? 50 : 200;
    if (arg != 0xFFFFFFFF) {
      /* update to absolute */
      arg = pcan_timestamp_millis() + arg;
    }
  }

  led_mode_array[led].arg = arg;
}

static void _led_update_state(int led, uint8_t state) {
  GPIO_PinState pin_state = state ? LED_ON : LED_OFF;

  switch (led) {
  case LED_STAT:
    HAL_GPIO_WritePin(LED_STAT_PORT, LED_STAT_PIN, pin_state);
    break;

  case LED_CH0_TX:
    HAL_GPIO_WritePin(LED_CH0_TX_PORT, LED_CH0_TX_PIN, pin_state);
    break;

  case LED_CH0_RX:
    HAL_GPIO_WritePin(LED_CH0_RX_PORT, LED_CH0_RX_PIN, pin_state);
    break;

  case LED_CH1_TX:
    HAL_GPIO_WritePin(LED_CH1_TX_PORT, LED_CH1_TX_PIN, pin_state);
    break;

  case LED_CH1_RX:
    HAL_GPIO_WritePin(LED_CH1_RX_PORT, LED_CH1_RX_PIN, pin_state);
    break;
  }
}

void pcan_led_poll(void) {
  uint32_t ts_ms = pcan_timestamp_millis();

  for (int i = 0; i < LED_TOTAL; i++) {
    if (!led_mode_array[i].timestamp)
      continue;
    if ((uint32_t)(ts_ms - led_mode_array[i].timestamp) <
        led_mode_array[i].delay)
      continue;

    switch (led_mode_array[i].mode) {
    default:
    case LED_MODE_DEVICE:
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_OFF:
    case LED_MODE_ON:
      led_mode_array[i].state = (led_mode_array[i].mode == LED_MODE_ON);
      led_mode_array[i].timestamp = 0;
      break;
    case LED_MODE_BLINK_FAST:
    case LED_MODE_BLINK_SLOW:
      led_mode_array[i].state ^= 1;
      led_mode_array[i].timestamp += led_mode_array[i].delay;
      if ((led_mode_array[i].arg != 0xFFFFFFFF) &&
          (led_mode_array[i].arg <= ts_ms)) {
        pcan_led_set_mode(i, LED_MODE_OFF, 0);
      }
      break;
    }

    _led_update_state(i, led_mode_array[i].state);
  }
}
#endif
