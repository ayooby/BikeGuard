#include <esp_sleep.h>
#include "sleep.h"
#include "config.h"
#include <Arduino.h>

void boardSleep() {
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_MS * 1000ULL);
    esp_light_sleep_start();
}
