#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_init(lv_display_t **display_out);
esp_err_t board_display_off(void);
i2c_master_bus_handle_t board_i2c_bus(void);

#ifdef __cplusplus
}
#endif
