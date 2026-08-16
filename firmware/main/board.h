#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_init(lv_display_t **display_out);
esp_err_t board_display_off(void);

#ifdef __cplusplus
}
#endif
