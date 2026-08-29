#include "funbox_audio.h"

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define AUDIO_RATE 16000
#define AUDIO_CHUNK 160
#define AUDIO_TASK_STACK 3072

typedef enum {
    AUDIO_CMD_BUTTON = 0,
    AUDIO_CMD_VOLUME,
    AUDIO_CMD_BACKGROUND,
    AUDIO_CMD_REWARD,
    AUDIO_CMD_STOP,
} audio_command_type_t;

typedef struct {
    audio_command_type_t type;
    uint8_t value;
} audio_command_t;

typedef struct {
    uint16_t frequency;
    uint8_t beats;
} melody_note_t;

/* Original wind-themed chiptune. It deliberately does not reproduce an
   existing song melody; short rising phrases keep the requested airy mood. */
static const melody_note_t BACKGROUND_MELODY[] = {
    { 523, 2 }, { 659, 2 }, { 784, 3 }, { 0, 1 },
    { 587, 2 }, { 698, 2 }, { 880, 3 }, { 0, 1 },
    { 659, 2 }, { 784, 2 }, { 988, 2 }, { 880, 2 },
    { 784, 3 }, { 659, 1 }, { 587, 2 }, { 523, 2 },
};

static const char *TAG = "funbox_audio";
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static volatile bool s_stopped = true;

static void send_command(audio_command_type_t type, uint8_t value)
{
    if (!s_queue) return;
    audio_command_t command = { .type = type, .value = value };
    xQueueSend(s_queue, &command, 0);
}

static void audio_task(void *arg)
{
    audio_command_t initial = *(audio_command_t *)arg;
    uint8_t volume = initial.value;
    bool background = initial.type == AUDIO_CMD_BACKGROUND;
    int16_t samples[AUDIO_CHUNK];
    uint32_t background_phase = 0;
    uint32_t effect_phase = 0;
    uint32_t note_samples = 0;
    uint32_t effect_samples = 0;
    uint16_t effect_frequency = 0;
    size_t note_index = sizeof(BACKGROUND_MELODY) / sizeof(BACKGROUND_MELODY[0]) - 1;

    if (bsp_audio_set_format(AUDIO_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio format setup failed");
        s_stopped = true;
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    bsp_audio_set_volume(volume);

    bool running = true;
    while (running) {
        audio_command_t command;
        while (xQueueReceive(s_queue, &command, 0) == pdTRUE) {
            if (command.type == AUDIO_CMD_STOP) running = false;
            if (command.type == AUDIO_CMD_VOLUME) {
                volume = command.value;
                bsp_audio_set_volume(volume);
            }
            if (command.type == AUDIO_CMD_BACKGROUND) background = command.value != 0;
            if (command.type == AUDIO_CMD_BUTTON) {
                static const uint16_t tones[] = { 880, 988, 1175 };
                effect_frequency = tones[command.value < 3 ? command.value : 2];
                effect_samples = AUDIO_RATE / 24;
                effect_phase = 0;
            }
            if (command.type == AUDIO_CMD_REWARD) {
                effect_frequency = 1568;
                effect_samples = AUDIO_RATE / 6;
                effect_phase = 0;
            }
        }
        if (!running) break;

        if (note_samples == 0) {
            note_index = (note_index + 1) %
                         (sizeof(BACKGROUND_MELODY) / sizeof(BACKGROUND_MELODY[0]));
            note_samples = (uint32_t)BACKGROUND_MELODY[note_index].beats * AUDIO_RATE / 8U;
            background_phase = 0;
        }

        const melody_note_t *note = &BACKGROUND_MELODY[note_index];
        for (size_t i = 0; i < AUDIO_CHUNK; i++) {
            int32_t sample = 0;
            if (effect_samples > 0) {
                sample = effect_phase < AUDIO_RATE / 2U ? 6200 : -6200;
                effect_phase = (effect_phase + effect_frequency) % AUDIO_RATE;
                effect_samples--;
            } else if (background && note->frequency) {
                uint32_t triangle = background_phase < AUDIO_RATE / 2U ?
                    background_phase : AUDIO_RATE - background_phase;
                sample = ((int32_t)triangle * 3200 / (AUDIO_RATE / 2U)) - 1600;
                background_phase = (background_phase + note->frequency) % AUDIO_RATE;
            }
            samples[i] = (int16_t)sample;
            if (note_samples > 0) note_samples--;
        }
        bsp_audio_write(samples, sizeof(samples));
    }

    s_stopped = true;
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t funbox_audio_start(uint8_t volume, bool background_enabled)
{
    if (s_task) return ESP_OK;
    s_queue = xQueueCreate(8, sizeof(audio_command_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    static audio_command_t initial;
    initial.type = background_enabled ? AUDIO_CMD_BACKGROUND : AUDIO_CMD_VOLUME;
    initial.value = volume;
    s_stopped = false;
    if (xTaskCreate(audio_task, "funbox_audio", AUDIO_TASK_STACK, &initial, 4, &s_task) != pdPASS) {
        s_stopped = true;
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void funbox_audio_stop(void)
{
    send_command(AUDIO_CMD_STOP, 0);
    for (int i = 0; i < 100 && !s_stopped; i++) vTaskDelay(pdMS_TO_TICKS(2));
    if (s_queue) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
}

void funbox_audio_button(bsp_btn_t button)
{
    send_command(AUDIO_CMD_BUTTON, (uint8_t)button);
}

void funbox_audio_set_volume(uint8_t volume)
{
    send_command(AUDIO_CMD_VOLUME, volume);
}

void funbox_audio_set_background(bool enabled)
{
    send_command(AUDIO_CMD_BACKGROUND, enabled ? 1 : 0);
}

void funbox_audio_reward(void)
{
    send_command(AUDIO_CMD_REWARD, 0);
}
