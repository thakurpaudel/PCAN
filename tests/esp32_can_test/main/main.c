#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "ESP32_CAN_TEST";

// Configuration from menuconfig
#define CAN_TX_PIN         CONFIG_TEST_CAN_TX_PIN
#define CAN_RX_PIN         CONFIG_TEST_CAN_RX_PIN
#define STATUS_LED_PIN     CONFIG_TEST_STATUS_LED_PIN
#define CAN_BITRATE_K      CONFIG_TEST_CAN_BITRATE_K
#define TX_INTERVAL_MS     CONFIG_TEST_CAN_TX_INTERVAL_MS
#define TX_MSG_ID          CONFIG_TEST_CAN_TX_MSG_ID

static bool led_state = false;

static void toggle_led(void)
{
#if STATUS_LED_PIN >= 0
    led_state = !led_state;
    gpio_set_level(STATUS_LED_PIN, led_state ? 1 : 0);
#endif
}

static twai_timing_config_t get_timing_config(uint32_t bitrate_kbps)
{
    switch (bitrate_kbps) {
        case 1000: return (twai_timing_config_t)TWAI_TIMING_CONFIG_1MBITS();
        case 800:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_800KBITS();
        case 500:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
        case 250:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_250KBITS();
        case 125:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_125KBITS();
        case 100:  return (twai_timing_config_t)TWAI_TIMING_CONFIG_100KBITS();
        case 50:   return (twai_timing_config_t)TWAI_TIMING_CONFIG_50KBITS();
        case 25:   return (twai_timing_config_t)TWAI_TIMING_CONFIG_25KBITS();
        default:
            ESP_LOGW(TAG, "Unsupported bitrate %lu kbps. Defaulting to 500 kbps.", (unsigned long)bitrate_kbps);
            return (twai_timing_config_t)TWAI_TIMING_CONFIG_500KBITS();
    }
}

static void can_rx_task(void *pvParameters)
{
    ESP_LOGI(TAG, "CAN RX task started, listening for messages...");
    twai_message_t message;

    while (1) {
        // Wait for a CAN message with infinite timeout
        esp_err_t err = twai_receive(&message, portMAX_DELAY);
        if (err == ESP_OK) {
            toggle_led();

            char data_str[32] = {0};
            if (!message.rtr) {
                int len = 0;
                for (int i = 0; i < message.data_length_code; i++) {
                    len += snprintf(data_str + len, sizeof(data_str) - len, "%02X ", message.data[i]);
                }
            } else {
                snprintf(data_str, sizeof(data_str), "RTR");
            }

            ESP_LOGI(TAG, "RX [ID: 0x%03lx %s] DLC: %d Data: %s",
                     (unsigned long)message.identifier,
                     message.extd ? "EXT" : "STD",
                     message.data_length_code,
                     data_str);
        } else {
            ESP_LOGE(TAG, "RX failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static void can_tx_task(void *pvParameters)
{
    ESP_LOGI(TAG, "CAN TX task started, interval: %d ms", TX_INTERVAL_MS);
    uint32_t counter = 0;
    twai_message_t tx_msg = {0};

    // Configure whether it's an extended or standard frame
    tx_msg.extd = (TX_MSG_ID > 0x7FF) ? 1 : 0;
    tx_msg.identifier = TX_MSG_ID;
    tx_msg.data_length_code = 8;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TX_INTERVAL_MS));

        // Prepare rolling counter data
        tx_msg.data[0] = (counter >> 24) & 0xFF;
        tx_msg.data[1] = (counter >> 16) & 0xFF;
        tx_msg.data[2] = (counter >> 8) & 0xFF;
        tx_msg.data[3] = counter & 0xFF;
        tx_msg.data[4] = 0xAA;
        tx_msg.data[5] = 0xBB;
        tx_msg.data[6] = 0xCC;
        tx_msg.data[7] = 0xDD;

        esp_err_t err = twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "TX [ID: 0x%03lx] Count: %lu", (unsigned long)tx_msg.identifier, (unsigned long)counter);
            toggle_led();
            counter++;
        } else {
            ESP_LOGW(TAG, "TX failed (maybe no ACK / bus-off): %s", esp_err_to_name(err));
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing CAN (TWAI) Test Program...");

    // Initialize Status LED
#if STATUS_LED_PIN >= 0
    ESP_LOGI(TAG, "Configuring Status LED on GPIO %d", STATUS_LED_PIN);
    gpio_reset_pin(STATUS_LED_PIN);
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, 0);
#endif

    // Setup TWAI Driver Configurations
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = get_timing_config(CAN_BITRATE_K);
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_LOGI(TAG, "TWAI: TX=GPIO %d, RX=GPIO %d, Bitrate=%d kbps", CAN_TX_PIN, CAN_RX_PIN, CAN_BITRATE_K);

    // Install and start TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TWAI driver: %s", esp_err_to_name(err));
        return;
    }

    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start TWAI driver: %s", esp_err_to_name(err));
        twai_driver_uninstall();
        return;
    }

    ESP_LOGI(TAG, "TWAI driver installed and started successfully.");

    // Create RX and TX tasks
    xTaskCreatePinnedToCore(can_rx_task, "can_rx_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(can_tx_task, "can_tx_task", 4096, NULL, 5, NULL, 1);
}
