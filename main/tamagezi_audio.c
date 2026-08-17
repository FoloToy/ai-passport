#include "tamagezi_audio.h"

#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define SAMPLE_RATE 16000U
#define CHUNK_SAMPLES 256U

typedef struct {
    uint8_t sound;
    uint8_t motif;
} sound_request_t;

static QueueHandle_t s_queue;
static volatile uint8_t s_level;
static uint32_t s_noise = 0xC001D00DU;

static int16_t wave_sample(uint32_t phase, uint8_t wave, int16_t amplitude)
{
    uint16_t point = (uint16_t)(phase >> 16);
    if (wave == 0) return point < 32768U ? amplitude : (int16_t)-amplitude;
    if (wave == 1) {
        int32_t triangle = point < 32768U ? (int32_t)point * 2 - 32768 :
                           98303 - (int32_t)point * 2;
        return (int16_t)(triangle * amplitude / 32768);
    }
    s_noise ^= s_noise << 13;
    s_noise ^= s_noise >> 17;
    s_noise ^= s_noise << 5;
    return (int16_t)(((int32_t)(s_noise & 0xFFFFU) - 32768) * amplitude / 32768);
}

static void play_tone(uint16_t frequency, uint16_t milliseconds, uint8_t wave)
{
    if (frequency == 0 || milliseconds == 0 || s_level == 0) return;
    uint32_t total = SAMPLE_RATE * milliseconds / 1000U;
    uint32_t step = (uint32_t)(((uint64_t)frequency << 32) / SAMPLE_RATE);
    uint32_t phase = 0;
    int16_t samples[CHUNK_SAMPLES];
    uint32_t rendered = 0;
    while (rendered < total) {
        size_t count = total - rendered < CHUNK_SAMPLES ? total - rendered : CHUNK_SAMPLES;
        for (size_t i = 0; i < count; i++) {
            uint32_t position = rendered + i;
            uint32_t edge = total / 8U + 1U;
            uint32_t envelope = position < edge ? position * 100U / edge :
                                total - position < edge ? (total - position) * 100U / edge : 100U;
            int16_t amplitude = (int16_t)((900 + s_level * 900) * envelope / 100U);
            samples[i] = wave_sample(phase, wave, amplitude);
            phase += step;
        }
        bsp_audio_write(samples, count * sizeof(samples[0]));
        rendered += count;
    }
}

static void pause_ms(uint16_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

static void play_request(const sound_request_t *request)
{
    static const uint16_t ROOTS[12] = {
        392, 440, 330, 262, 349, 294, 523, 311, 415, 370, 277, 494,
    };
    uint16_t root = ROOTS[request->motif % 12U];
    uint8_t wave = request->motif % 3U;
    switch ((tmz_sound_t)request->sound) {
    case TMZ_SOUND_MOVE: play_tone(root, 35, wave); break;
    case TMZ_SOUND_SELECT:
        play_tone(root, 55, wave); play_tone(root * 5U / 4U, 65, wave); break;
    case TMZ_SOUND_FEED:
        play_tone(root, 45, 2); pause_ms(25); play_tone(root * 3U / 2U, 70, wave); break;
    case TMZ_SOUND_CLEAN:
        play_tone(root * 2U, 35, 2); play_tone(root * 3U / 2U, 45, 2); break;
    case TMZ_SOUND_COIN:
        play_tone(988, 55, 0); pause_ms(25); play_tone(1319, 100, 0); break;
    case TMZ_SOUND_SUCCESS:
        play_tone(root, 70, wave); play_tone(root * 5U / 4U, 70, wave);
        play_tone(root * 3U / 2U, 130, wave); break;
    case TMZ_SOUND_FAIL:
        play_tone(root, 110, 0); play_tone(root * 3U / 4U, 180, 0); break;
    case TMZ_SOUND_GROW:
        for (uint8_t i = 0; i < 4; i++) {
            play_tone(root + i * 110U, 90, i % 2U);
        }
        break;
    case TMZ_SOUND_FUSION:
        for (uint8_t i = 0; i < 7; i++) {
            play_tone(root + i * 75U, 55, 2);
        }
        play_tone(root * 2U, 220, wave); break;
    case TMZ_SOUND_CALL:
        play_tone(root * 3U / 2U, 90, wave); pause_ms(80);
        play_tone(root * 3U / 2U, 90, wave); break;
    }
}

static void audio_task(void *context)
{
    (void)context;
    sound_request_t request;
    bsp_audio_set_format(SAMPLE_RATE, 16, 1);
    while (true) {
        if (xQueueReceive(s_queue, &request, portMAX_DELAY) == pdTRUE && s_level > 0) {
            play_request(&request);
        }
    }
}

bool tmz_audio_start(uint8_t sound_level)
{
    s_level = sound_level > 3 ? 2 : sound_level;
    if (bsp_audio_init() != ESP_OK) return false;
    bsp_audio_set_volume(55);
    if (s_queue) return true;
    s_queue = xQueueCreate(6, sizeof(sound_request_t));
    if (!s_queue) return false;
    if (xTaskCreate(audio_task, "tmz_audio", 3072, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return false;
    }
    return true;
}

void tmz_audio_set_level(uint8_t sound_level)
{
    s_level = sound_level > 3 ? 3 : sound_level;
}

void tmz_audio_play(tmz_sound_t sound, uint8_t motif)
{
    if (!s_queue || s_level == 0) return;
    sound_request_t request = { .sound = sound, .motif = motif };
    xQueueSend(s_queue, &request, 0);
}
