#include "game_state.h"

#include "esp_log.h"

#define YUGIOH_STARTING_LIFE 8000U
#define MTG_STARTING_LIFE 20U

static const char *TAG = "game_state";

void game_state_init(game_state_t *state)
{
    state->mode = GAME_MODE_NONE;
    state->p1 = 0;
    state->p2 = 0;
    ESP_LOGI(TAG, "Waiting for game selection");
}

void game_state_reset(game_state_t *state, game_mode_t mode)
{
    uint32_t life = mode == GAME_MODE_MTG ? MTG_STARTING_LIFE : YUGIOH_STARTING_LIFE;
    state->mode = mode;
    state->p1 = life;
    state->p2 = life;
    ESP_LOGI(TAG, "Started %s game: Ben=%lu, Player2=%lu",
             mode == GAME_MODE_MTG ? "MTG" : "Yu-Gi-Oh",
             (unsigned long)life, (unsigned long)life);
}

void game_state_save(const game_state_t *state)
{
    /* Format and life totals are session-only: every boot starts with the chooser. */
    (void)state;
}
