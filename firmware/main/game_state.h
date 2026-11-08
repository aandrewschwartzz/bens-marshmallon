#pragma once

#include <stdint.h>

typedef enum {
    GAME_MODE_NONE = 0,
    GAME_MODE_YUGIOH,
    GAME_MODE_MTG,
} game_mode_t;

typedef struct {
    game_mode_t mode;
    uint32_t p1;
    uint32_t p2;
} game_state_t;

void game_state_init(game_state_t *state);
void game_state_reset(game_state_t *state, game_mode_t mode);
void game_state_save(const game_state_t *state);
