#include "pcanpro_timestamp.h"
#include "esp_timer.h"

void pcan_timestamp_init(void) {
    // Already initialized by ESP system
}

uint32_t pcan_timestamp_millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint32_t pcan_timestamp_us(void) {
    return (uint32_t)esp_timer_get_time();
}
