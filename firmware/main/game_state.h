#pragma once

#include <stdint.h>

typedef struct {
    uint32_t p1;
    uint32_t p2;
} game_state_t;

void game_state_init(game_state_t *state);
void game_state_save(const game_state_t *state);
