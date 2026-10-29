#include "ui.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "audio.h"
#include "lvgl.h"
#include "marshmallon_assets.h"

#define COLOR_RIM 0x9A5335
#define COLOR_HEADER 0xD59A6D
#define COLOR_ART 0xF6E89A
#define COLOR_DARK 0x33251D
#define COLOR_INK 0x20130D
#define COLOR_MUTED_INK 0x60483B
#define COLOR_CREAM 0xFFF5D9
#define COLOR_FOOTER 0xF2DDBD
#define COLOR_RED 0xD9422E
#define COLOR_GREEN 0x54A95F
#define COLOR_GOLD 0xF0A82F

#define DEFAULT_LIFE_POINTS 8000U
#define MAX_LIFE_POINTS 99999U
#define TOUCH_SAFE_MARGIN 16
#define COIN_CENTER_X 175
#define COIN_BASE_Y 76
#define COIN_FULL_WIDTH 76
#define COIN_HEIGHT 76
#define COIN_FLIP_FRAMES 30

typedef enum {
    PLAYER_BEN = 0,
    PLAYER_TWO = 1,
} player_t;

typedef enum {
    CHANGE_LOSS = 0,
    CHANGE_GAIN = 1,
} change_mode_t;

typedef enum {
    COIN_OFFER = 0,
    COIN_FLIPPING = 1,
    COIN_RESULT = 2,
} coin_state_t;

static const char *TAG = "ui";

static game_state_t s_state;
static player_t s_selected = PLAYER_BEN;
static change_mode_t s_mode = CHANGE_LOSS;
static char s_amount[6];

static lv_obj_t *s_home;
static lv_obj_t *s_amount_screen;
static lv_obj_t *s_review_screen;
static lv_obj_t *s_game_over;
static lv_obj_t *s_coin_screen;

static lv_obj_t *s_p1_lp;
static lv_obj_t *s_p2_lp;
static lv_obj_t *s_pet;
static lv_obj_t *s_pet_status;
static lv_obj_t *s_reaction_badge;
static lv_obj_t *s_reaction_text;

static lv_obj_t *s_amount_value;
static lv_obj_t *s_review_button;
static lv_obj_t *s_review_player;
static lv_obj_t *s_review_amount;
static lv_obj_t *s_loss_action;
static lv_obj_t *s_loss_result;
static lv_obj_t *s_gain_action;
static lv_obj_t *s_gain_result;

static lv_obj_t *s_winner_name;
static lv_obj_t *s_final_score;
static lv_obj_t *s_final_pet;

static lv_obj_t *s_coin_title;
static lv_obj_t *s_coin_subtitle;
static lv_obj_t *s_coin_pet;
static lv_obj_t *s_coin;
static lv_obj_t *s_coin_inner;
static lv_obj_t *s_coin_face;
static lv_obj_t *s_coin_detail;
static lv_obj_t *s_coin_yes;
static lv_obj_t *s_coin_no;
static lv_obj_t *s_coin_start;
static lv_obj_t *s_coin_sparkle_left;
static lv_obj_t *s_coin_sparkle_right;
static coin_state_t s_coin_state = COIN_OFFER;
static player_t s_coin_result = PLAYER_BEN;
static int s_coin_anim_face = -1;
static int s_coin_frame;

static lv_obj_t *s_battery_chip;
static lv_obj_t *s_battery_fill;
static lv_obj_t *s_battery_bolt;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_warning_scrim;
static lv_obj_t *s_warning_detail;
static int s_last_warning_level;

static lv_timer_t *s_reaction_timer;
static lv_timer_t *s_coin_flip_timer;
static lv_timer_t *s_coin_result_timer;

static lv_color_t color(uint32_t hex)
{
    return lv_color_hex(hex);
}

static void make_plain(lv_obj_t *object)
{
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_outline_width(object, 0, 0);
    lv_obj_set_style_shadow_width(object, 0, 0);
}

static lv_obj_t *make_box(lv_obj_t *parent, int x, int y, int width, int height,
                          uint32_t background, uint32_t border, int border_width, int radius)
{
    lv_obj_t *box = lv_obj_create(parent);
    make_plain(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, color(background), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, radius, 0);
    if (border_width > 0) {
        lv_obj_set_style_border_color(box, color(border), 0);
        lv_obj_set_style_border_width(box, border_width, 0);
        lv_obj_set_style_border_opa(box, LV_OPA_COVER, 0);
    }
    return box;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int x, int y, int width,
                            const lv_font_t *font, uint32_t text_color, lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(text_color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static lv_obj_t *make_button(lv_obj_t *parent, int x, int y, int width, int height,
                             uint32_t background, uint32_t border, int radius,
                             const char *text, const lv_font_t *font, uint32_t text_color)
{
    lv_obj_t *button = lv_button_create(parent);
    make_plain(button);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, color(background), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, radius, 0);
    lv_obj_set_style_border_color(button, color(border), 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(text_color), 0);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *make_image(lv_obj_t *parent, const lv_image_dsc_t *source, int x, int y)
{
    lv_obj_t *image = lv_image_create(parent);
    lv_image_set_src(image, source);
    lv_obj_set_pos(image, x, y);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return image;
}

static void show_only(lv_obj_t *screen)
{
    lv_obj_add_flag(s_home, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_amount_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_review_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_game_over, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_screen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_life_labels(void)
{
    lv_label_set_text_fmt(s_p1_lp, "%lu", (unsigned long)s_state.p1);
    lv_label_set_text_fmt(s_p2_lp, "%lu", (unsigned long)s_state.p2);
}

static const char *selected_name(void)
{
    return s_selected == PLAYER_BEN ? "BEN" : "PLAYER 2";
}

static uint32_t *selected_points(void)
{
    return s_selected == PLAYER_BEN ? &s_state.p1 : &s_state.p2;
}

static bool amount_is_actionable(void)
{
    return s_amount[0] && strtoul(s_amount, NULL, 10) > 0;
}

static void update_amount_screen(void)
{
    uint32_t displayed_amount = s_amount[0] ? (uint32_t)strtoul(s_amount, NULL, 10) : 0;
    lv_label_set_text_fmt(s_amount_value, "%lu", (unsigned long)displayed_amount);
    lv_label_set_text_fmt(lv_obj_get_child(s_review_button, 0), "NEXT: %s", selected_name());
    if (displayed_amount == 0) {
        lv_obj_add_state(s_review_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_review_button, color(0xA88F78), 0);
    } else {
        lv_obj_remove_state(s_review_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_review_button, color(0x6E3425), 0);
    }
}

static void update_review_screen(void)
{
    uint32_t amount = (uint32_t)strtoul(s_amount, NULL, 10);
    uint32_t current = *selected_points();
    uint32_t after_loss = amount >= current ? 0 : current - amount;
    uint32_t after_gain = current > MAX_LIFE_POINTS - amount ? MAX_LIFE_POINTS : current + amount;

    lv_label_set_text_fmt(s_review_player, "%s CURRENTLY HAS %lu LP",
                          selected_name(), (unsigned long)current);
    lv_label_set_text_fmt(s_review_amount, "CHANGE BY %lu", (unsigned long)amount);
    lv_label_set_text_fmt(s_loss_action, "RIGHT: LOSE %lu", (unsigned long)amount);
    lv_label_set_text_fmt(s_loss_result, "Life Points become %lu", (unsigned long)after_loss);
    lv_label_set_text_fmt(s_gain_action, "LEFT: GAIN %lu", (unsigned long)amount);
    lv_label_set_text_fmt(s_gain_result, "Life Points become %lu", (unsigned long)after_gain);
}

static void show_home(void)
{
    show_only(s_home);
}

static void set_coin_face(player_t player, bool mystery)
{
    if (mystery) {
        lv_obj_set_style_bg_color(s_coin_inner, color(0x6B2E74), 0);
        lv_label_set_text(s_coin_face, "?");
        lv_label_set_text(s_coin_detail, "DUEL COIN");
        return;
    }

    bool ben = player == PLAYER_BEN;
    lv_obj_set_style_bg_color(s_coin_inner, color(ben ? 0xC8902F : 0x6B2E74), 0);
    lv_label_set_text(s_coin_face, ben ? "BEN" : "P2");
    lv_label_set_text(s_coin_detail, ben ? "FIRST" : "FIRST");
}

static void set_coin_geometry(int width, int y)
{
    if (width < 8) {
        width = 8;
    }
    int inner_width = width > 12 ? width - 12 : 2;
    lv_obj_set_pos(s_coin, COIN_CENTER_X - width / 2, y);
    lv_obj_set_size(s_coin, width, COIN_HEIGHT);
    lv_obj_set_style_radius(s_coin, width / 2, 0);
    lv_obj_set_pos(s_coin_inner, (width - inner_width) / 2, 6);
    lv_obj_set_size(s_coin_inner, inner_width, 64);
    lv_obj_set_style_radius(s_coin_inner, inner_width / 2, 0);
    lv_obj_set_width(s_coin_face, inner_width);
    lv_obj_set_width(s_coin_detail, inner_width);

    if (width < 28) {
        lv_obj_add_flag(s_coin_face, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_coin_detail, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_coin_face, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_coin_detail, LV_OBJ_FLAG_HIDDEN);
    }
}

static void leave_coin_screen(void)
{
    if (s_coin_result_timer) {
        lv_timer_delete(s_coin_result_timer);
        s_coin_result_timer = NULL;
    }
    show_home();
}

static void coin_result_timeout(lv_timer_t *timer)
{
    (void)timer;
    s_coin_result_timer = NULL;
    ESP_LOGI(TAG, "Coin result complete; starting duel");
    show_home();
}

static void finish_coin_flip(void)
{
    s_coin_state = COIN_RESULT;
    set_coin_geometry(COIN_FULL_WIDTH, COIN_BASE_Y);
    set_coin_face(s_coin_result, false);
    lv_label_set_text_fmt(s_coin_title, "%s GOES FIRST!",
                          s_coin_result == PLAYER_BEN ? "BEN" : "PLAYER 2");
    lv_label_set_text(s_coin_subtitle, "START DUEL OR WAIT 3 SECONDS");
    lv_obj_remove_flag(s_coin_sparkle_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_coin_sparkle_right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_coin_start, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Coin landed: %s goes first",
             s_coin_result == PLAYER_BEN ? "Ben" : "Player 2");

    s_coin_result_timer = lv_timer_create(coin_result_timeout, 3000, NULL);
    lv_timer_set_repeat_count(s_coin_result_timer, 1);
}

static void coin_flip_tick(lv_timer_t *timer)
{
    static const uint8_t widths[] = {76, 50, 22, 8, 22, 50};
    ++s_coin_frame;
    if (s_coin_frame >= COIN_FLIP_FRAMES) {
        lv_timer_delete(timer);
        s_coin_flip_timer = NULL;
        finish_coin_flip();
        return;
    }

    int32_t progress = (s_coin_frame * 1000) / COIN_FLIP_FRAMES;
    int32_t arc = (int32_t)((int64_t)24 * 4 * progress * (1000 - progress) / 1000000);
    int phase = s_coin_frame % (int)(sizeof(widths) / sizeof(widths[0]));
    int face = ((s_coin_frame + 3) / 6) & 1;
    set_coin_geometry(widths[phase], COIN_BASE_Y - arc);
    if (face != s_coin_anim_face) {
        s_coin_anim_face = face;
        set_coin_face(face ? PLAYER_TWO : PLAYER_BEN, false);
    }
}

static void show_coin_offer(void)
{
    if (s_coin_flip_timer) {
        lv_timer_delete(s_coin_flip_timer);
        s_coin_flip_timer = NULL;
    }
    if (s_coin_result_timer) {
        lv_timer_delete(s_coin_result_timer);
        s_coin_result_timer = NULL;
    }
    s_coin_state = COIN_OFFER;
    s_coin_anim_face = -1;
    s_coin_frame = 0;
    set_coin_geometry(COIN_FULL_WIDTH, COIN_BASE_Y);
    set_coin_face(PLAYER_BEN, true);
    lv_label_set_text(s_coin_title, "NEW DUEL");
    lv_label_set_text(s_coin_subtitle, "FLIP TO CHOOSE WHO GOES FIRST?");
    lv_obj_remove_flag(s_coin_yes, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_coin_no, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_start, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_sparkle_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_sparkle_right, LV_OBJ_FLAG_HIDDEN);
    show_only(s_coin_screen);
}

static void start_coin_flip(void)
{
    if (s_coin_state != COIN_OFFER) {
        return;
    }

    s_coin_state = COIN_FLIPPING;
    s_coin_result = (esp_random() & 1U) ? PLAYER_TWO : PLAYER_BEN;
    s_coin_anim_face = -1;
    s_coin_frame = 0;
    lv_label_set_text(s_coin_title, "FLIPPING...");
    lv_label_set_text(s_coin_subtitle, "THE DUEL COIN IS IN THE AIR");
    lv_obj_add_flag(s_coin_yes, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_no, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Coin flip started");
    s_coin_flip_timer = lv_timer_create(coin_flip_tick, 50, NULL);
}

static void reset_game_and_offer_coin(void)
{
    s_state.p1 = DEFAULT_LIFE_POINTS;
    s_state.p2 = DEFAULT_LIFE_POINTS;
    refresh_life_labels();
    game_state_save(&s_state);
    show_coin_offer();
}

static void show_amount(player_t player)
{
    s_selected = player;
    s_mode = CHANGE_LOSS;
    s_amount[0] = '\0';
    update_amount_screen();
    show_only(s_amount_screen);
}

static bool show_review(void)
{
    if (!amount_is_actionable()) {
        return false;
    }
    update_review_screen();
    show_only(s_review_screen);
    return true;
}

static void activate_player_shortcut(player_t player)
{
    bool entering_amount = !lv_obj_has_flag(s_amount_screen, LV_OBJ_FLAG_HIDDEN);
    if (entering_amount && s_selected == player) {
        /* A second press of the active player's physical button mirrors Next.
         * With no valid amount, leave the input intact and wait for a value. */
        (void)show_review();
        return;
    }
    show_amount(player);
}

static void show_game_over(void)
{
    bool ben_won = s_state.p2 == 0;
    lv_label_set_text(s_winner_name, ben_won ? "BEN WINS!" : "PLAYER 2 WINS");
    lv_label_set_text_fmt(s_final_score, "%lu - %lu", (unsigned long)s_state.p1, (unsigned long)s_state.p2);
    lv_image_set_src(s_final_pet, ben_won ? &marshmallon_win : &marshmallon_lose);
    show_only(s_game_over);
}

static void clear_reaction(lv_timer_t *timer)
{
    (void)timer;
    s_reaction_timer = NULL;
    lv_obj_set_style_translate_x(s_pet, 0, 0);
    lv_obj_set_style_translate_y(s_pet, 0, 0);
    lv_image_set_rotation(s_pet, 0);
    lv_image_set_scale(s_pet, LV_SCALE_NONE);
    lv_label_set_text(s_pet_status, "");
    lv_obj_add_flag(s_reaction_badge, LV_OBJ_FLAG_HIDDEN);
    if (s_state.p1 == 0 || s_state.p2 == 0) {
        show_game_over();
    }
}

static void set_pet_wiggle(void *object, int32_t progress)
{
    float phase = (progress / 1000.0f) * 6.2831853f * 3.0f;
    float fade = 1.0f - (progress / 1000.0f);
    int32_t angle = (int32_t)lroundf(sinf(phase) * 90.0f * fade);
    lv_image_set_rotation((lv_obj_t *)object, angle);
}

static void pet_clicked(lv_event_t *event)
{
    audio_play_jiggle();
    lv_obj_t *pet = lv_event_get_current_target(event);
    lv_anim_delete(pet, set_pet_wiggle);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, pet);
    lv_anim_set_exec_cb(&animation, set_pet_wiggle);
    lv_anim_set_values(&animation, 0, 1000);
    lv_anim_set_duration(&animation, 650);
    lv_anim_start(&animation);
}

static void begin_reaction(const char *status, uint32_t badge_color, int x_offset, int y_offset, uint32_t delta)
{
    if (s_reaction_timer) {
        lv_timer_delete(s_reaction_timer);
        s_reaction_timer = NULL;
    }
    lv_label_set_text(s_pet_status, status);
    lv_label_set_text_fmt(s_reaction_text, "%c%lu", s_mode == CHANGE_LOSS ? '-' : '+', (unsigned long)delta);
    lv_obj_set_style_bg_color(s_reaction_badge, color(badge_color), 0);
    lv_obj_remove_flag(s_reaction_badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_translate_x(s_pet, x_offset, 0);
    lv_obj_set_style_translate_y(s_pet, y_offset, 0);
    s_reaction_timer = lv_timer_create(clear_reaction, 900, NULL);
    lv_timer_set_repeat_count(s_reaction_timer, 1);
}

static void player_card_clicked(lv_event_t *event)
{
    player_t player = (player_t)(intptr_t)lv_event_get_user_data(event);
    show_amount(player);
}

static void back_clicked(lv_event_t *event)
{
    (void)event;
    show_home();
}

static void amount_swiped(lv_event_t *event)
{
    (void)event;
    lv_indev_t *input = lv_indev_active();
    if (input && lv_indev_get_gesture_dir(input) == LV_DIR_RIGHT) {
        show_home();
    }
}

static void review_clicked(lv_event_t *event)
{
    (void)event;
    (void)show_review();
}

static void review_swiped(lv_event_t *event)
{
    (void)event;
    lv_indev_t *input = lv_indev_active();
    if (input && lv_indev_get_gesture_dir(input) == LV_DIR_RIGHT) {
        show_only(s_amount_screen);
    }
}

static void key_clicked(lv_event_t *event)
{
    const char *key = (const char *)lv_event_get_user_data(event);
    size_t current = strlen(s_amount);
    size_t added = strlen(key);
    if (current + added > 5) {
        return;
    }
    strncat(s_amount, key, sizeof(s_amount) - current - 1);
    while (s_amount[0] == '0') {
        memmove(s_amount, s_amount + 1, strlen(s_amount));
    }
    update_amount_screen();
}

static void delete_clicked(lv_event_t *event)
{
    (void)event;
    size_t length = strlen(s_amount);
    if (length > 0) {
        s_amount[length - 1] = '\0';
    }
    update_amount_screen();
}

static void apply_change(change_mode_t mode)
{
    s_mode = mode;
    uint32_t delta = s_amount[0] ? (uint32_t)strtoul(s_amount, NULL, 10) : 0;
    if (delta == 0) {
        return;
    }

    uint32_t *points = selected_points();
    if (s_mode == CHANGE_LOSS) {
        *points = delta >= *points ? 0 : *points - delta;
    } else {
        *points = *points > MAX_LIFE_POINTS - delta ? MAX_LIFE_POINTS : *points + delta;
    }
    refresh_life_labels();
    game_state_save(&s_state);

    bool opponent_hit = s_mode == CHANGE_LOSS && s_selected == PLAYER_TWO;
    bool ben_hit = s_mode == CHANGE_LOSS && s_selected == PLAYER_BEN;
    bool ben_gain = s_mode == CHANGE_GAIN && s_selected == PLAYER_BEN;

    const char *status;
    uint32_t badge_color;
    int x_offset = 0;
    int y_offset = 0;
    if (opponent_hit) {
        status = delta >= 2000 ? "HUGE HIT!" : "NICE HIT!";
        badge_color = COLOR_GOLD;
        y_offset = -8;
    } else if (ben_hit) {
        status = delta >= 2000 ? "OH NO, BEN!" : "BEN!";
        badge_color = COLOR_RED;
        x_offset = -8;
    } else if (ben_gain) {
        status = "FEELING BETTER!";
        badge_color = COLOR_GREEN;
        y_offset = -8;
    } else {
        status = "UH-OH...";
        badge_color = COLOR_RED;
        y_offset = 6;
    }

    show_home();
    begin_reaction(status, badge_color, x_offset, y_offset, delta);
}

static void apply_clicked(lv_event_t *event)
{
    apply_change((change_mode_t)(intptr_t)lv_event_get_user_data(event));
}

static void new_game_clicked(lv_event_t *event)
{
    (void)event;
    reset_game_and_offer_coin();
}

static void coin_yes_clicked(lv_event_t *event)
{
    (void)event;
    start_coin_flip();
}

static void coin_no_clicked(lv_event_t *event)
{
    (void)event;
    if (s_coin_state == COIN_OFFER) {
        ESP_LOGI(TAG, "Coin flip skipped");
        leave_coin_screen();
    }
}

static void coin_start_clicked(lv_event_t *event)
{
    (void)event;
    leave_coin_screen();
}

static lv_obj_t *create_player_card(lv_obj_t *parent, int y, player_t player,
                                    const char *name, lv_obj_t **lp_label)
{
    lv_obj_t *card = make_button(parent, 18, y, 96, 47, COLOR_CREAM, 0x5B3826, 5,
                                 "", &lv_font_montserrat_12, COLOR_INK);
    lv_obj_add_event_cb(card, player_card_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)player);
    make_label(card, name, 8, 5, 76, &lv_font_montserrat_10, COLOR_MUTED_INK, LV_TEXT_ALIGN_LEFT);
    *lp_label = make_label(card, "8000", 7, 18, 75, &lv_font_montserrat_24, COLOR_INK, LV_TEXT_ALIGN_LEFT);
    make_label(card, ">", 78, 19, 12, &lv_font_montserrat_16, 0x5B3826, LV_TEXT_ALIGN_CENTER);
    return card;
}

static void create_home_screen(lv_obj_t *root)
{
    s_home = make_box(root, 0, 0, 240, 240, COLOR_RIM, COLOR_RIM, 0, 0);
    make_box(s_home, 7, 5, 226, 29, COLOR_HEADER, 0x3A2117, 1, 3);
    make_label(s_home, "BEN'S MARSHMALLON", 13, 12, 169, &lv_font_montserrat_12, COLOR_INK, LV_TEXT_ALIGN_LEFT);

    make_box(s_home, 7, 39, 226, 141, COLOR_DARK, COLOR_DARK, 0, 2);
    make_box(s_home, 11, 43, 218, 133, COLOR_ART, COLOR_ART, 0, 0);

    s_pet = make_image(s_home, &marshmallon_home, 120, 58);
    lv_image_set_pivot(s_pet, 52, 68);
    lv_obj_add_flag(s_pet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_pet, pet_clicked, LV_EVENT_CLICKED, NULL);
    create_player_card(s_home, 50, PLAYER_TWO, "PLAYER 2", &s_p2_lp);
    create_player_card(s_home, 122, PLAYER_BEN, "BEN - P1", &s_p1_lp);

    s_pet_status = make_label(s_home, "", 117, 163, 112, &lv_font_montserrat_10, 0x4A2D20, LV_TEXT_ALIGN_CENTER);
    s_reaction_badge = make_box(s_home, 145, 101, 54, 25, COLOR_RED, 0x7A2117, 1, 12);
    s_reaction_text = make_label(s_reaction_badge, "-0", 2, 5, 50, &lv_font_montserrat_12, 0xFFF8DF, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(s_reaction_badge, LV_OBJ_FLAG_HIDDEN);

    make_box(s_home, 7, 184, 226, 49, COLOR_FOOTER, 0x4B2B1D, 1, 2);
    make_label(s_home, "[FAIRY / EFFECT]", 12, 190, 150, &lv_font_montserrat_10, COLOR_INK, LV_TEXT_ALIGN_LEFT);
    make_label(s_home, "Tap duelist for LP. Tap Marshmallon!", 12, 204, 216, &lv_font_montserrat_10, 0x5D493D, LV_TEXT_ALIGN_LEFT);
    make_box(s_home, 12, 218, 216, 1, 0x6A4633, 0x6A4633, 0, 0);
    make_label(s_home, "ATK / 300   DEF / 500", 12, 221, 216, &lv_font_montserrat_10, COLOR_INK, LV_TEXT_ALIGN_RIGHT);
}

static void create_amount_screen(lv_obj_t *root)
{
    s_amount_screen = make_box(root, 0, 0, 240, 240, COLOR_RIM, COLOR_RIM, 0, 0);
    lv_obj_add_event_cb(s_amount_screen, amount_swiped, LV_EVENT_GESTURE, NULL);

    make_box(s_amount_screen, TOUCH_SAFE_MARGIN, 16, 144, 32, COLOR_CREAM, 0x5B3826, 1, 8);
    s_amount_value = make_label(s_amount_screen, "0", 22, 19, 130,
                                &lv_font_montserrat_20, COLOR_INK, LV_TEXT_ALIGN_RIGHT);
    lv_obj_t *delete_button = make_button(s_amount_screen, 168, 16, 56, 32,
                                          COLOR_CREAM, 0x5B3826, 8, "DEL",
                                          &lv_font_montserrat_10, COLOR_INK);
    lv_obj_add_event_cb(delete_button, delete_clicked, LV_EVENT_CLICKED, NULL);

    static const char *keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
    static const int key_x[] = {16, 88, 160, 16, 88, 160, 16, 88, 160};
    static const int key_y[] = {54, 54, 54, 88, 88, 88, 122, 122, 122};
    for (int i = 0; i < 9; ++i) {
        lv_obj_t *key = make_button(s_amount_screen, key_x[i], key_y[i], 64, 30,
                                    COLOR_CREAM, 0x5B3826, 7, keys[i],
                                    &lv_font_montserrat_14, COLOR_INK);
        lv_obj_add_event_cb(key, key_clicked, LV_EVENT_CLICKED, (void *)keys[i]);
    }

    lv_obj_t *back = make_button(s_amount_screen, 16, 156, 64, 30,
                                 COLOR_CREAM, 0x5B3826, 7, "BACK",
                                 &lv_font_montserrat_10, COLOR_INK);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *zero = make_button(s_amount_screen, 88, 156, 136, 30,
                                 COLOR_CREAM, 0x5B3826, 7, "0",
                                 &lv_font_montserrat_14, COLOR_INK);
    lv_obj_add_event_cb(zero, key_clicked, LV_EVENT_CLICKED, (void *)"0");

    s_review_button = make_button(s_amount_screen, 16, 190, 208, 42,
                                  0xA88F78, 0x5B3826, 7, "NEXT",
                                  &lv_font_montserrat_14, COLOR_CREAM);
    /* Keep the visible primary action and its touch target identical.  The
     * taller target is easier to acquire near the display's bottom bezel. */
    lv_obj_remove_flag(s_review_button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(s_review_button, review_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_amount_screen, LV_OBJ_FLAG_HIDDEN);
}

static void create_review_screen(lv_obj_t *root)
{
    s_review_screen = make_box(root, 0, 0, 240, 240, COLOR_RIM, COLOR_RIM, 0, 0);
    lv_obj_add_event_cb(s_review_screen, review_swiped, LV_EVENT_GESTURE, NULL);

    s_review_player = make_label(s_review_screen, "BEN CURRENTLY HAS 8000 LP", 10, 18, 220,
                                 &lv_font_montserrat_10, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    s_review_amount = make_label(s_review_screen, "CHANGE BY 0", 10, 38, 220,
                                 &lv_font_montserrat_20, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *loss = make_button(s_review_screen, 18, 75, 204, 62,
                                 0xA63C2D, 0x572017, 10, "", &lv_font_montserrat_14, COLOR_CREAM);
    s_loss_action = make_label(loss, "RIGHT: LOSE 0", 8, 10, 188,
                               &lv_font_montserrat_16, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    s_loss_result = make_label(loss, "Life Points become 8000", 8, 37, 188,
                               &lv_font_montserrat_10, 0xFFE0D8, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(loss, apply_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)CHANGE_LOSS);

    lv_obj_t *gain = make_button(s_review_screen, 18, 162, 204, 62,
                                 0x3E844D, 0x214A2A, 10, "", &lv_font_montserrat_14, COLOR_CREAM);
    s_gain_action = make_label(gain, "LEFT: GAIN 0", 8, 10, 188,
                               &lv_font_montserrat_16, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    s_gain_result = make_label(gain, "Life Points become 8000", 8, 37, 188,
                               &lv_font_montserrat_10, 0xE2FFE6, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_event_cb(gain, apply_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)CHANGE_GAIN);
    lv_obj_add_flag(s_review_screen, LV_OBJ_FLAG_HIDDEN);
}

static void create_game_over_screen(lv_obj_t *root)
{
    s_game_over = make_box(root, 0, 0, 240, 240, COLOR_RIM, COLOR_RIM, 0, 0);
    make_label(s_game_over, "GAME OVER", 12, 9, 166, &lv_font_montserrat_16, COLOR_INK, LV_TEXT_ALIGN_LEFT);
    make_box(s_game_over, 7, 35, 226, 157, COLOR_DARK, COLOR_DARK, 0, 2);
    make_box(s_game_over, 11, 39, 218, 149, COLOR_ART, COLOR_ART, 0, 0);
    s_winner_name = make_label(s_game_over, "BEN WINS!", 20, 45, 200, &lv_font_montserrat_20, COLOR_INK, LV_TEXT_ALIGN_CENTER);
    s_final_pet = make_image(s_game_over, &marshmallon_win, 74, 56);
    lv_image_set_pivot(s_final_pet, 46, 80);
    s_final_score = make_label(s_game_over, "8000 - 0", 20, 172, 200, &lv_font_montserrat_14, COLOR_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *new_game = make_button(s_game_over, 24, 196, 192, 38,
                                     0x6E3425, 0x5B3826, 10, "NEW GAME",
                                     &lv_font_montserrat_14, COLOR_CREAM);
    lv_obj_add_event_cb(new_game, new_game_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_game_over, LV_OBJ_FLAG_HIDDEN);
}

static void create_coin_screen(lv_obj_t *root)
{
    s_coin_screen = make_box(root, 0, 0, 240, 240, COLOR_RIM, COLOR_RIM, 0, 0);
    make_box(s_coin_screen, 7, 35, 226, 137, COLOR_DARK, COLOR_DARK, 0, 3);
    make_box(s_coin_screen, 11, 39, 218, 129, 0xE8C77B, 0xE8C77B, 0, 1);

    s_coin_title = make_label(s_coin_screen, "NEW DUEL", 12, 9, 160,
                              &lv_font_montserrat_16, COLOR_INK, LV_TEXT_ALIGN_LEFT);
    s_coin_subtitle = make_label(s_coin_screen, "FLIP TO CHOOSE WHO GOES FIRST?", 16, 43, 208,
                                 &lv_font_montserrat_10, COLOR_INK, LV_TEXT_ALIGN_CENTER);

    /* Marshmallon remains the host of the new-duel scene; the coin is his
     * prop, not a replacement for the character. */
    s_coin_pet = make_image(s_coin_screen, &marshmallon_home, 18, 53);

    s_coin = make_box(s_coin_screen, 137, COIN_BASE_Y, COIN_FULL_WIDTH, COIN_HEIGHT,
                      COLOR_GOLD, 0x3A2117, 4, 38);
    lv_obj_remove_flag(s_coin, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_shadow_color(s_coin, color(0x5A2A17), 0);
    lv_obj_set_style_shadow_opa(s_coin, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(s_coin, 8, 0);
    lv_obj_set_style_shadow_offset_y(s_coin, 5, 0);

    s_coin_inner = make_box(s_coin, 6, 6, 64, 64, 0x6B2E74, 0xFFF0A6, 2, 32);
    lv_obj_remove_flag(s_coin_inner, LV_OBJ_FLAG_CLICKABLE);
    s_coin_face = make_label(s_coin_inner, "?", 0, 12, 64,
                             &lv_font_montserrat_20, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    s_coin_detail = make_label(s_coin_inner, "DUEL COIN", 0, 39, 64,
                               &lv_font_montserrat_10, 0xFFF0A6, LV_TEXT_ALIGN_CENTER);

    s_coin_sparkle_left = make_label(s_coin_screen, "*", 124, 91, 20,
                                     &lv_font_montserrat_20, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    s_coin_sparkle_right = make_label(s_coin_screen, "*", 211, 73, 20,
                                      &lv_font_montserrat_20, COLOR_CREAM, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(s_coin_sparkle_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_sparkle_right, LV_OBJ_FLAG_HIDDEN);

    s_coin_yes = make_button(s_coin_screen, 16, 184, 100, 40,
                             0x6B2E74, 0x3A183F, 9, "LEFT: FLIP",
                             &lv_font_montserrat_10, COLOR_CREAM);
    lv_obj_add_event_cb(s_coin_yes, coin_yes_clicked, LV_EVENT_CLICKED, NULL);
    s_coin_no = make_button(s_coin_screen, 124, 184, 100, 40,
                            COLOR_CREAM, 0x5B3826, 9, "RIGHT: SKIP",
                            &lv_font_montserrat_10, COLOR_INK);
    lv_obj_add_event_cb(s_coin_no, coin_no_clicked, LV_EVENT_CLICKED, NULL);
    s_coin_start = make_button(s_coin_screen, 24, 184, 192, 40,
                               0x6E3425, 0x5B3826, 9, "START DUEL",
                               &lv_font_montserrat_14, COLOR_CREAM);
    lv_obj_add_event_cb(s_coin_start, coin_start_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_coin_start, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_coin_screen, LV_OBJ_FLAG_HIDDEN);
}

static void create_battery_chip(void)
{
    s_battery_chip = make_box(lv_layer_top(), 179, 5, 57, 22, 0x2B1B12, 0xFFF5D9, 1, 7);
    lv_obj_t *outline = make_box(s_battery_chip, 4, 6, 18, 10, 0x2B1B12, 0xFFF5D9, 1, 2);
    make_box(s_battery_chip, 22, 9, 2, 4, 0xFFF5D9, 0xFFF5D9, 0, 1);
    s_battery_fill = make_box(outline, 2, 2, 12, 4, COLOR_GREEN, COLOR_GREEN, 0, 1);
    s_battery_bolt = make_label(outline, LV_SYMBOL_CHARGE, 5, -1, 8,
                                &lv_font_montserrat_10, 0xFFD43D, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_flag(s_battery_bolt, LV_OBJ_FLAG_HIDDEN);
    s_battery_label = make_label(s_battery_chip, "--", 25, 4, 29,
                                 &lv_font_montserrat_10, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
}

static void warning_dismissed(lv_event_t *event)
{
    (void)event;
    if (s_warning_scrim) {
        lv_obj_delete(s_warning_scrim);
        s_warning_scrim = NULL;
        s_warning_detail = NULL;
    }
}

static void show_battery_warning(const battery_status_t *battery, int level)
{
    if (s_warning_scrim) {
        if (s_warning_detail) {
            lv_label_set_text_fmt(s_warning_detail, "%d%%  -  %.2f V", battery->percent, battery->voltage);
        }
        return;
    }

    s_warning_scrim = make_box(lv_layer_top(), 0, 0, 240, 240, 0x000000, 0x000000, 0, 0);
    lv_obj_set_style_bg_opa(s_warning_scrim, LV_OPA_60, 0);
    lv_obj_t *card = make_box(s_warning_scrim, 15, 61, 210, 118, COLOR_CREAM, 0x5B3826, 2, 12);
    make_label(card, level >= 2 ? "CRITICAL BATTERY" : "LOW BATTERY", 10, 12, 190,
               &lv_font_montserrat_18, level >= 2 ? COLOR_RED : 0x9A6500, LV_TEXT_ALIGN_CENTER);
    s_warning_detail = make_label(card, "", 10, 41, 190,
                                  &lv_font_montserrat_16, COLOR_INK, LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(s_warning_detail, "%d%%  -  %.2f V", battery->percent, battery->voltage);
    make_label(card, "Connect USB power soon.", 10, 66, 190,
               &lv_font_montserrat_12, COLOR_MUTED_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *dismiss = make_button(card, 67, 88, 76, 24, 0x6E3425, 0x5B3826, 7,
                                    "GOT IT", &lv_font_montserrat_10, COLOR_CREAM);
    lv_obj_add_event_cb(dismiss, warning_dismissed, LV_EVENT_CLICKED, NULL);
}

void ui_update_battery(const battery_status_t *battery)
{
    if (!battery || !s_battery_chip) {
        return;
    }

    if (!battery->present) {
        lv_label_set_text(s_battery_label, "USB");
        lv_obj_set_width(s_battery_fill, 2);
        lv_obj_set_style_bg_color(s_battery_fill, color(0x8F9AA5), 0);
        if (battery->usb_power) {
            lv_obj_remove_flag(s_battery_bolt, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_battery_bolt, LV_OBJ_FLAG_HIDDEN);
        }
        s_last_warning_level = 0;
        return;
    }

    int fill_width = 2 + (battery->percent * 10) / 100;
    if (fill_width > 12) {
        fill_width = 12;
    }
    lv_obj_set_width(s_battery_fill, fill_width);

    uint32_t fill_color = battery->percent <= 10 ? COLOR_RED :
                          battery->percent <= 20 ? COLOR_GOLD : COLOR_GREEN;
    lv_obj_set_style_bg_color(s_battery_fill, color(fill_color), 0);
    lv_label_set_text_fmt(s_battery_label, "%d%%", battery->percent);
    if (battery->usb_power) {
        lv_obj_remove_flag(s_battery_bolt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_battery_bolt, LV_OBJ_FLAG_HIDDEN);
    }

    int warning_level = battery->percent <= 10 ? 2 : battery->percent <= 20 ? 1 : 0;
    if (warning_level == 0 || battery->usb_power) {
        s_last_warning_level = 0;
        return;
    }
    if (warning_level > s_last_warning_level) {
        s_last_warning_level = warning_level;
        show_battery_warning(battery, warning_level);
    }
}

void ui_handle_left_button(void)
{
    if (!lv_obj_has_flag(s_coin_screen, LV_OBJ_FLAG_HIDDEN)) {
        if (s_coin_state == COIN_OFFER) {
            start_coin_flip();
        } else if (s_coin_state == COIN_RESULT) {
            leave_coin_screen();
        }
        return;
    }
    if (!lv_obj_has_flag(s_review_screen, LV_OBJ_FLAG_HIDDEN)) {
        apply_change(CHANGE_GAIN);
        return;
    }
    activate_player_shortcut(PLAYER_TWO);
}

void ui_handle_right_button(void)
{
    if (!lv_obj_has_flag(s_coin_screen, LV_OBJ_FLAG_HIDDEN)) {
        if (s_coin_state == COIN_OFFER || s_coin_state == COIN_RESULT) {
            if (s_coin_state == COIN_OFFER) {
                ESP_LOGI(TAG, "Coin flip skipped");
            }
            leave_coin_screen();
        }
        return;
    }
    if (!lv_obj_has_flag(s_review_screen, LV_OBJ_FLAG_HIDDEN)) {
        apply_change(CHANGE_LOSS);
        return;
    }
    activate_player_shortcut(PLAYER_BEN);
}

void ui_create(game_state_t initial_state, const battery_status_t *battery)
{
    s_state = initial_state;
    lv_obj_t *root = lv_screen_active();
    make_plain(root);
    lv_obj_set_style_bg_color(root, color(COLOR_RIM), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    create_home_screen(root);
    create_amount_screen(root);
    create_review_screen(root);
    create_game_over_screen(root);
    create_coin_screen(root);
    create_battery_chip();
    ui_update_battery(battery);
    reset_game_and_offer_coin();
    ESP_LOGI(TAG, "UI ready: fresh duel, coin flip offered");
}
