#include "game_state.h"

#include "esp_log.h"

#define DEFAULT_LIFE_POINTS 8000

static const char *TAG = "game_state";

void game_state_init(game_state_t *state)
{
    state->p1 = DEFAULT_LIFE_POINTS;
    state->p2 = DEFAULT_LIFE_POINTS;
    ESP_LOGI(TAG, "Started fresh LP state: Ben=8000, Player2=8000");
}

void game_state_save(const game_state_t *state)
{
    /* Duel state is intentionally session-only: every boot starts a new game. */
    (void)state;
}
