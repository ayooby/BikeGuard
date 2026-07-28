#include <esp_sleep.h>
#include "sleep.h"
#include "config.h"
#include <Arduino.h>

void boardDeepSleep(uint32_t seconds) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(MPU_INT_PIN), 1);
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL);

    Serial.printf("[SLEEP] deep sleep for up to %u s (or shake wake)\n", static_cast<unsigned>(seconds));
    delay(50);
    esp_deep_sleep_start();
}
