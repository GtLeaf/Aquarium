#include "hal_audio.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hal_audio";

// 蜂鸣器引脚（ESP32-S3-Touch-AMOLED-1.8 通常使用 GPIO 40 或 41）
#define BUZZER_GPIO     40
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_SPEED    LEDC_LOW_SPEED_MODE

static bool s_muted = false;

// 音符频率表（简化）
static const uint16_t note_freqs[] = {
    0,      // NONE
    800,    // CLICK
    600,    // FEED
    1000,   // EVENT
    1200,   // REWARD
    1500,   // UNLOCK
    400,    // WARNING
};

// 音符时长(ms)
static const uint16_t note_durations[] = {
    0, 50, 100, 200, 300, 400, 150,
};

esp_err_t hal_audio_init(void)
{
    ESP_LOGI(TAG, "Initializing audio (PWM buzzer on GPIO %d)", BUZZER_GPIO);

    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_SPEED,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BUZZER_TIMER,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = BUZZER_SPEED,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));

    return ESP_OK;
}

void hal_audio_play(enum sound_effect effect)
{
    if (s_muted || effect == SOUND_NONE) return;
    if (effect >= sizeof(note_freqs) / sizeof(note_freqs[0])) return;

    uint16_t freq = note_freqs[effect];
    uint16_t duration = note_durations[effect];

    if (freq == 0) return;

    // 设置频率和占空比（50%）
    ledc_set_freq(BUZZER_SPEED, BUZZER_TIMER, freq);
    ledc_set_duty(BUZZER_SPEED, BUZZER_CHANNEL, 512);
    ledc_update_duty(BUZZER_SPEED, BUZZER_CHANNEL);

    // 延时后关闭
    vTaskDelay(pdMS_TO_TICKS(duration));

    ledc_set_duty(BUZZER_SPEED, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_SPEED, BUZZER_CHANNEL);

    ESP_LOGD(TAG, "Played sound effect %d: freq=%d, dur=%d", effect, freq, duration);
}

void hal_audio_set_mute(bool mute)
{
    s_muted = mute;
    ESP_LOGI(TAG, "Audio %s", mute ? "muted" : "unmuted");
}

bool hal_audio_is_muted(void)
{
    return s_muted;
}
