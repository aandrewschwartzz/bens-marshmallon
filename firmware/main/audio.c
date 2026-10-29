#include "audio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SAMPLE_RATE 22050
#define CODEC_ADDR 0x18
#define AMP_PIN GPIO_NUM_7
#define I2S_MCLK_PIN GPIO_NUM_8
#define I2S_BCLK_PIN GPIO_NUM_9
#define I2S_WS_PIN GPIO_NUM_10
#define I2S_DOUT_PIN GPIO_NUM_12
#define AUDIO_CHUNK_FRAMES 128
#define RESTART_FADE_FRAMES 128

static const char *TAG = "jiggle_audio";
static i2s_chan_handle_t s_tx;
static TaskHandle_t s_task;
static _Atomic uint32_t s_request_id;

extern const uint8_t jiggle_start[] asm("_binary_jiggle_pcm_start");
extern const uint8_t jiggle_end[] asm("_binary_jiggle_pcm_end");

static esp_err_t codec_write(i2c_master_dev_handle_t codec, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(codec, data, sizeof(data), 100);
}

static esp_err_t codec_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CODEC_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t codec;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &config, &codec), TAG, "codec I2C setup failed");

    /* Waveshare's ES8311 setup, using the board's real MCLK on GPIO8.
       MCLK = 256 * 22050 Hz; the DAC runs in 16-bit Philips I2S slave mode. */
    ESP_RETURN_ON_ERROR(codec_write(codec, 0x00, 0x1f), TAG, "codec reset failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    static const uint8_t setup[][2] = {
        {0x00, 0x00}, {0x00, 0x80},
        {0x01, 0x3f}, {0x02, 0x00}, {0x03, 0x10}, {0x04, 0x10},
        {0x05, 0x00}, {0x06, 0x04}, {0x07, 0x00}, {0x08, 0xff},
        {0x09, 0x0c}, {0x0a, 0x0c},
        {0x0d, 0x01}, {0x0e, 0x02}, {0x12, 0x00}, {0x13, 0x10},
        {0x1c, 0x6a}, {0x37, 0x08},
        {0x32, 0xbf}, {0x31, 0x00},
    };
    for (size_t i = 0; i < sizeof(setup) / sizeof(setup[0]); ++i) {
        ESP_RETURN_ON_ERROR(codec_write(codec, setup[i][0], setup[i][1]), TAG, "codec register write failed");
    }
    return ESP_OK;
}

static bool write_stereo(const int16_t *stereo, size_t frames)
{
    size_t bytes = frames * 2 * sizeof(int16_t);
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(s_tx, stereo, bytes, &bytes_written, portMAX_DELAY);
    if (err != ESP_OK || bytes_written != bytes) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void audio_task(void *arg)
{
    (void)arg;
    const size_t frames = (size_t)(jiggle_end - jiggle_start) / sizeof(int16_t);
    const int16_t *samples = (const int16_t *)jiggle_start;
    int16_t stereo[AUDIO_CHUNK_FRAMES * 2];
    uint32_t playing_id = 0;
    size_t pos = frames;
    while (true) {
        uint32_t newest_id = atomic_load_explicit(&s_request_id, memory_order_relaxed);
        if (newest_id != playing_id) {
            if (pos < frames) {
                /* Continue the old waveform into the new attack over 5.8 ms.
                   Short DMA chunks bound tap-to-restart latency. */
                for (size_t i = 0; i < RESTART_FADE_FRAMES; ++i) {
                    int32_t old_sample = pos + i < frames ? samples[pos + i] : 0;
                    int32_t new_sample = samples[i];
                    int16_t mixed = (int16_t)((old_sample * (RESTART_FADE_FRAMES - i) +
                                               new_sample * i) / RESTART_FADE_FRAMES);
                    stereo[2 * i] = mixed;
                    stereo[2 * i + 1] = mixed;
                }
                if (!write_stereo(stereo, RESTART_FADE_FRAMES)) {
                    pos = frames;
                    playing_id = newest_id;
                    continue;
                }
                pos = RESTART_FADE_FRAMES;
            } else {
                pos = 0;
            }
            playing_id = newest_id;
        }
        if (pos == frames) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        size_t count = frames - pos;
        if (count > AUDIO_CHUNK_FRAMES) count = AUDIO_CHUNK_FRAMES;
        for (size_t i = 0; i < count; ++i) {
            stereo[2 * i] = samples[pos + i];
            stereo[2 * i + 1] = samples[pos + i];
        }
        if (write_stereo(stereo, count)) {
            pos += count;
        } else {
            pos = frames;
        }
    }
}

esp_err_t audio_init(i2c_master_bus_handle_t bus)
{
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_ARG, TAG, "I2C bus missing");

    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = 3;
    channel.dma_frame_num = AUDIO_CHUNK_FRAMES;
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, &s_tx, NULL), TAG, "I2S channel failed");

    i2s_std_config_t standard = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_PIN,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_WS_PIN,
            .dout = I2S_DOUT_PIN,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(s_tx, &standard), TAG, "I2S mode failed");
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_tx), TAG, "I2S enable failed");
    ESP_RETURN_ON_ERROR(codec_init(bus), TAG, "ES8311 init failed");

    gpio_config_t amp = {
        .pin_bit_mask = 1ULL << AMP_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&amp), TAG, "speaker amplifier GPIO failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(AMP_PIN, 1), TAG, "speaker amplifier enable failed");

    BaseType_t created = xTaskCreatePinnedToCore(audio_task, "jiggle_audio", 4096, NULL,
                                                  4, &s_task, 1);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "audio task failed");
    ESP_LOGI(TAG, "ES8311 ready: %d Hz, %u frames", SAMPLE_RATE,
             (unsigned)((jiggle_end - jiggle_start) / sizeof(int16_t)));
    return ESP_OK;
}

void audio_play_jiggle(void)
{
    if (s_task != NULL) {
        atomic_fetch_add_explicit(&s_request_id, 1, memory_order_relaxed);
        xTaskNotifyGive(s_task);
    }
}
