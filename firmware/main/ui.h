#pragma once

#include "battery.h"
#include "game_state.h"

void ui_create(game_state_t initial_state, const battery_status_t *battery);
void ui_update_battery(const battery_status_t *battery);
void ui_handle_left_button(void);
void ui_handle_right_button(void);
