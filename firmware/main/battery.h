#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    float voltage;
    int percent;
    bool charging;
    bool usb_power;
    bool present;
} battery_status_t;

esp_err_t battery_init(void);
esp_err_t battery_read(battery_status_t *status);
esp_err_t battery_power_off(void);
