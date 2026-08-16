#include "battery.h"

#include <math.h>

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BATTERY_ADC_PIN GPIO_NUM_1
#define BATTERY_ENABLE_PIN GPIO_NUM_2
#define CHARGE_STATUS_PIN GPIO_NUM_3
#define BATTERY_DIVIDER_RATIO 3.0f
#define BATTERY_SAMPLE_COUNT 24

static const char *TAG = "battery";
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_calibration;
static bool s_calibrated;

static int voltage_to_percent(float voltage)
{
    static const float volts[] = {3.45f, 3.52f, 3.64f, 3.76f, 3.88f, 4.00f, 4.20f};
    static const int percents[] = {0, 5, 20, 40, 60, 80, 100};
    const int points = sizeof(volts) / sizeof(volts[0]);

    if (voltage <= volts[0]) {
        return 0;
    }
    if (voltage >= volts[points - 1]) {
        return 100;
    }
    for (int i = 1; i < points; ++i) {
        if (voltage <= volts[i]) {
            float portion = (voltage - volts[i - 1]) / (volts[i] - volts[i - 1]);
            return percents[i - 1] + (int)lroundf(portion * (percents[i] - percents[i - 1]));
        }
    }
    return 100;
}

esp_err_t battery_init(void)
{
    gpio_config_t charge_input = {
        .pin_bit_mask = 1ULL << CHARGE_STATUS_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&charge_input), TAG, "charge-status GPIO failed");

    gpio_config_t battery_enable = {
        .pin_bit_mask = 1ULL << BATTERY_ENABLE_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&battery_enable), TAG, "battery-enable GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(BATTERY_ENABLE_PIN, 1), TAG, "battery-enable failed");

    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &s_adc), TAG, "ADC unit failed");

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &channel_config), TAG, "ADC channel failed");

    adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t calibration_result = adc_cali_create_scheme_curve_fitting(&calibration_config, &s_calibration);
    s_calibrated = calibration_result == ESP_OK;
    if (!s_calibrated) {
        ESP_LOGW(TAG, "ADC calibration unavailable; using raw-voltage estimate");
    }
    ESP_LOGI(TAG, "Battery monitor ready on GPIO1 (3:1 divider), charge status GPIO3");
    return ESP_OK;
}

esp_err_t battery_read(battery_status_t *status)
{
    ESP_RETURN_ON_FALSE(status != NULL, ESP_ERR_INVALID_ARG, TAG, "status is required");
    int64_t millivolt_sum = 0;

    for (int i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
        int raw = 0;
        ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw), TAG, "ADC read failed");
        int millivolts = 0;
        if (s_calibrated) {
            ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(s_calibration, raw, &millivolts), TAG, "ADC conversion failed");
        } else {
            millivolts = (raw * 3300) / 4095;
        }
        millivolt_sum += millivolts;
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    float adc_voltage = (millivolt_sum / (float)BATTERY_SAMPLE_COUNT) / 1000.0f;
    status->voltage = adc_voltage * BATTERY_DIVIDER_RATIO;
    status->present = status->voltage >= 2.80f && status->voltage <= 4.45f;
    status->percent = status->present ? voltage_to_percent(status->voltage) : 0;
    status->charging = status->present && status->percent < 100 && gpio_get_level(CHARGE_STATUS_PIN) == 0;
    status->usb_power = status->charging || usb_serial_jtag_is_connected() || !status->present;
    return ESP_OK;
}

esp_err_t battery_power_off(void)
{
    ESP_LOGI(TAG, "Releasing battery power latch");
    return gpio_set_level(BATTERY_ENABLE_PIN, 0);
}
