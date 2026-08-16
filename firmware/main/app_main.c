#include "battery.h"
#include "board.h"
#include "game_state.h"
#include "ui.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEFT_BUTTON_PIN GPIO_NUM_0
#define CENTER_BUTTON_PIN GPIO_NUM_5
#define RIGHT_BUTTON_PIN GPIO_NUM_4
#define BUTTON_DEBOUNCE_MS 40
#define POWER_HOLD_MS 1500

static const char *TAG = "marshmallon";

typedef struct {
    gpio_num_t pin;
    bool raw_pressed;
    bool stable_pressed;
    bool armed;
    bool action_fired;
    TickType_t raw_changed_at;
    TickType_t pressed_at;
} physical_button_t;

static void handle_side_button(bool right)
{
    if (lvgl_port_lock(0)) {
        if (right) {
            ui_handle_right_button();
        } else {
            ui_handle_left_button();
        }
        lvgl_port_unlock();
    }
    ESP_LOGI(TAG, "Physical shortcut: %s button", right ? "right" : "left");
}

static void power_down(void)
{
    ESP_LOGI(TAG, "Center button held: powering down");
    if (lvgl_port_lock(0)) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(board_display_off());
        lvgl_port_unlock();
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(battery_power_off());

    /* Battery power drops when the key is released. USB cannot be cut in
       software, so a USB-powered board falls through into deep sleep. */
    while (gpio_get_level(CENTER_BUTTON_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    esp_err_t wake_result = esp_sleep_enable_ext1_wakeup_io(
        1ULL << CENTER_BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
    if (wake_result != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure center-button wake: %s", esp_err_to_name(wake_result));
        esp_restart();
    }
    ESP_LOGI(TAG, "USB power remains; sleeping until center button press");
    esp_deep_sleep_start();
}

static void init_button_state(physical_button_t *button, gpio_num_t pin)
{
    button->pin = pin;
    button->raw_pressed = gpio_get_level(pin) == 0;
    button->stable_pressed = button->raw_pressed;
    button->armed = !button->stable_pressed;
    button->action_fired = false;
    button->raw_changed_at = xTaskGetTickCount();
    button->pressed_at = button->raw_changed_at;
}

static bool update_button(physical_button_t *button, TickType_t now)
{
    bool pressed = gpio_get_level(button->pin) == 0;
    if (pressed != button->raw_pressed) {
        button->raw_pressed = pressed;
        button->raw_changed_at = now;
    }

    if (button->stable_pressed != button->raw_pressed &&
        now - button->raw_changed_at >= pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        button->stable_pressed = button->raw_pressed;
        if (button->stable_pressed) {
            if (button->armed) {
                button->pressed_at = now;
                button->action_fired = false;
                return true;
            }
        } else {
            button->armed = true;
            button->action_fired = false;
        }
    }
    return false;
}

static void controls_task(void *argument)
{
    (void)argument;
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << LEFT_BUTTON_PIN) |
                        (1ULL << CENTER_BUTTON_PIN) |
                        (1ULL << RIGHT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    physical_button_t left = {0};
    physical_button_t center = {0};
    physical_button_t right = {0};
    init_button_state(&left, LEFT_BUTTON_PIN);
    init_button_state(&center, CENTER_BUTTON_PIN);
    init_button_state(&right, RIGHT_BUTTON_PIN);
    ESP_LOGI(TAG, "Buttons ready: left=Player 2/gain, right=Ben/lose, hold center=power off");

    while (true) {
        TickType_t now = xTaskGetTickCount();
        if (update_button(&left, now)) {
            handle_side_button(false);
        }
        if (update_button(&right, now)) {
            handle_side_button(true);
        }
        update_button(&center, now);
        if (center.stable_pressed && center.armed && !center.action_fired &&
            now - center.pressed_at >= pdMS_TO_TICKS(POWER_HOLD_MS)) {
            center.action_fired = true;
            power_down();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void battery_task(void *argument)
{
    (void)argument;
    while (true) {
        battery_status_t status = {0};
        if (battery_read(&status) == ESP_OK) {
            ESP_LOGI(TAG, "Battery: %.3f V, %d%%, charging=%s, usb_power=%s, present=%s",
                     status.voltage,
                     status.percent,
                     status.charging ? "yes" : "no",
                     status.usb_power ? "yes" : "no",
                     status.present ? "yes" : "no");
            lvgl_port_lock(0);
            ui_update_battery(&status);
            lvgl_port_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Ben's Marshmallon");

    game_state_t state;
    game_state_init(&state);

    ESP_ERROR_CHECK(battery_init());
    battery_status_t battery = {0};
    ESP_ERROR_CHECK(battery_read(&battery));

    lv_display_t *display = NULL;
    ESP_ERROR_CHECK(board_init(&display));
    (void)display;

    lvgl_port_lock(0);
    ui_create(state, &battery);
    lvgl_port_unlock();

    xTaskCreate(battery_task, "battery_monitor", 4096, NULL, 3, NULL);
    xTaskCreate(controls_task, "physical_controls", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Ready: touch a duelist to change life points");
}
