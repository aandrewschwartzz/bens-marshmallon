#include "board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define LCD_H_RES 240
#define LCD_V_RES 240
#define LCD_HOST SPI3_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_DRAW_BUFFER_HEIGHT 50

#define LCD_PIN_SCLK GPIO_NUM_38
#define LCD_PIN_MOSI GPIO_NUM_39
#define LCD_PIN_RST GPIO_NUM_40
#define LCD_PIN_DC GPIO_NUM_45
#define LCD_PIN_CS GPIO_NUM_21
#define LCD_PIN_BL GPIO_NUM_46

#define TOUCH_I2C_PORT 0
#define TOUCH_PIN_SCL GPIO_NUM_41
#define TOUCH_PIN_SDA GPIO_NUM_42
#define TOUCH_PIN_INT GPIO_NUM_48
#define TOUCH_PIN_RST GPIO_NUM_47

static const char *TAG = "board";

static esp_lcd_panel_io_handle_t s_lcd_io;
static esp_lcd_panel_handle_t s_lcd_panel;
static esp_lcd_touch_handle_t s_touch;
static i2c_master_bus_handle_t s_i2c_bus;

static esp_err_t init_lcd(void)
{
    gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&backlight), TAG, "backlight GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BL, 0), TAG, "backlight off failed");

    spi_bus_config_t bus_config = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_DRAW_BUFFER_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .dc_gpio_num = LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_lcd_io),
        TAG,
        "panel IO failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_lcd_io, &panel_config, &s_lcd_panel), TAG, "ST7789 init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd_panel), TAG, "LCD reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd_panel), TAG, "LCD panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_panel, true), TAG, "LCD inversion failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel, true), TAG, "LCD display-on failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BL, 1), TAG, "backlight on failed");
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_i2c_bus), TAG, "touch I2C init failed");

    esp_lcd_panel_io_i2c_config_t touch_io_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .flags.disable_control_phase = true,
        .scl_speed_hz = 400 * 1000,
    };
    esp_lcd_panel_io_handle_t touch_io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &touch_io_config, &touch_io), TAG, "touch IO failed");

    esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_PIN_RST,
        /* Polling reliably captures the release between quick repeated taps. */
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
    };
    return esp_lcd_touch_new_i2c_cst816s(touch_io, &touch_config, &s_touch);
}

static esp_err_t init_lvgl(lv_display_t **display_out)
{
    lvgl_port_cfg_t port_config = {
        .task_priority = 4,
        .task_stack = 10 * 1024,
        .task_affinity = -1,
        .task_max_sleep_ms = 250,
        .timer_period_ms = 5,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_config), TAG, "LVGL port init failed");

    lvgl_port_display_cfg_t display_config = {
        .io_handle = s_lcd_io,
        .panel_handle = s_lcd_panel,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUFFER_HEIGHT,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    lv_display_t *display = lvgl_port_add_disp(&display_config);
    ESP_RETURN_ON_FALSE(display != NULL, ESP_FAIL, TAG, "LVGL display registration failed");

    lvgl_port_touch_cfg_t touch_config = {
        .disp = display,
        .handle = s_touch,
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_config) != NULL, ESP_FAIL, TAG, "LVGL touch registration failed");
    *display_out = display;
    return ESP_OK;
}

esp_err_t board_init(lv_display_t **display_out)
{
    ESP_RETURN_ON_FALSE(display_out != NULL, ESP_ERR_INVALID_ARG, TAG, "display_out is required");
    ESP_LOGI(TAG, "Initializing Waveshare ESP32-S3 Touch LCD 1.54");
    ESP_RETURN_ON_ERROR(init_lcd(), TAG, "LCD init failed");
    ESP_RETURN_ON_ERROR(init_touch(), TAG, "touch init failed");
    ESP_RETURN_ON_ERROR(init_lvgl(display_out), TAG, "LVGL init failed");
    ESP_LOGI(TAG, "Display and touch ready");
    return ESP_OK;
}

esp_err_t board_display_off(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BL, 0), TAG, "backlight off failed");
    if (s_lcd_panel) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel, false), TAG, "display off failed");
    }
    return ESP_OK;
}

i2c_master_bus_handle_t board_i2c_bus(void)
{
    return s_i2c_bus;
}
