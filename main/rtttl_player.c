#include "rtttl_player.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define RTTTL_SAMPLE_RATE   16000
#define RTTTL_CHUNK_SAMPLES 256
#define RTTTL_VOLUME        58
#define RTTTL_AMPLITUDE     5200

static const char *TAG = "rtttl";
static QueueHandle_t s_queue;
static TaskHandle_t s_task;
static volatile uint32_t s_generation;
static volatile bool s_playing;

typedef struct {
    const char *song;
    uint32_t generation;
} rtttl_request_t;

typedef struct {
    uint16_t duration;
    uint8_t octave;
    uint16_t bpm;
} rtttl_defaults_t;

static const char *skip_spaces(const char *text) {
    while (*text && isspace((unsigned char)*text)) text++;
    return text;
}
static const char *parse_number(const char *text, unsigned *value) {
    unsigned parsed = 0;
    bool found = false;
    while (isdigit((unsigned char)*text)) {
        parsed = parsed * 10U + (unsigned)(*text - '0');
        text++;
        found = true;
    }
    if (found) *value = parsed;
    return text;
}

static const char *parse_defaults(const char *song, rtttl_defaults_t *defaults) {
    defaults->duration = 4;
    defaults->octave = 6;
    defaults->bpm = 63;

    const char *text = song;
    while (*text && *text != ':') text++;
    if (*text != ':') return NULL;
    text++;

    while (*text && *text != ':') {
        text = skip_spaces(text);
        char key = (char)tolower((unsigned char)*text++);
        if (*text != '=') {
            while (*text && *text != ',' && *text != ':') text++;
        } else {
            text++;
            unsigned value = 0;
            text = parse_number(text, &value);
            if (key == 'd' && value > 0 && value <= 64) defaults->duration = (uint16_t)value;
            if (key == 'o' && value >= 2 && value <= 7) defaults->octave = (uint8_t)value;
            if (key == 'b' && value >= 25 && value <= 900) defaults->bpm = (uint16_t)value;
        }
        if (*text == ',') text++;
    }
    return *text == ':' ? text + 1 : NULL;
}

static uint16_t note_frequency(char note, bool sharp, uint8_t octave) {
    static const uint16_t OCTAVE_4[] = {
        262, 277, 294, 311, 330, 349,
        370, 392, 415, 440, 466, 494,
    };
    int semitone;
    switch (note) {
        case 'c': semitone = 0; break;
        case 'd': semitone = 2; break;
        case 'e': semitone = 4; break;
        case 'f': semitone = 5; break;
        case 'g': semitone = 7; break;
        case 'a': semitone = 9; break;
        case 'b': semitone = 11; break;
        default: return 0;
    }
    if (sharp) semitone++;
    if (semitone >= 12) {
        semitone -= 12;
        octave++;
    }
    uint32_t frequency = OCTAVE_4[semitone];
    while (octave > 4) {
        frequency *= 2U;
        octave--;
    }
    while (octave < 4) {
        frequency /= 2U;
        octave++;
    }
    return (uint16_t)frequency;
}

static void write_samples(int16_t *buffer, size_t count) {
    if (count > 0) bsp_audio_write(buffer, count * sizeof(buffer[0]));
}

static bool play_note(uint16_t frequency, uint32_t duration_ms, uint32_t generation) {
    int16_t buffer[RTTTL_CHUNK_SAMPLES];
    uint32_t sound_samples = frequency ?
        (uint32_t)((uint64_t)RTTTL_SAMPLE_RATE * duration_ms * 85U / 100000U) : 0;
    uint32_t total_samples = (uint32_t)((uint64_t)RTTTL_SAMPLE_RATE * duration_ms / 1000U);
    uint32_t phase = 0;
    uint32_t written = 0;

    while (written < total_samples) {
        if (generation != s_generation) return false;
        size_t count = total_samples - written;
        if (count > RTTTL_CHUNK_SAMPLES) count = RTTTL_CHUNK_SAMPLES;
        for (size_t i = 0; i < count; i++) {
            if (written + i >= sound_samples || frequency == 0) {
                buffer[i] = 0;
            } else {
                buffer[i] = phase < RTTTL_SAMPLE_RATE / 2U ? RTTTL_AMPLITUDE : -RTTTL_AMPLITUDE;
                phase += frequency;
                if (phase >= RTTTL_SAMPLE_RATE) phase -= RTTTL_SAMPLE_RATE;
            }
        }
        write_samples(buffer, count);
        written += (uint32_t)count;
    }
    return true;
}

static void play_song(const char *song, uint32_t generation) {
    rtttl_defaults_t defaults;
    const char *text = parse_defaults(song, &defaults);
    if (!text) {
        ESP_LOGW(TAG, "invalid RTTTL string");
        return;
    }

    uint32_t whole_note_ms = 240000U / defaults.bpm;
    while (*text) {
        text = skip_spaces(text);
        unsigned duration = defaults.duration;
        if (isdigit((unsigned char)*text)) text = parse_number(text, &duration);
        if (duration == 0) duration = defaults.duration;

        char note = (char)tolower((unsigned char)*text++);
        bool sharp = false;
        bool dotted = false;
        uint8_t octave = defaults.octave;
        if (*text == '#') {
            sharp = true;
            text++;
        }
        while (*text && *text != ',') {
            if (*text == '.') dotted = true;
            else if (isdigit((unsigned char)*text)) octave = (uint8_t)(*text - '0');
            text++;
        }

        uint32_t duration_ms = whole_note_ms / duration;
        if (dotted) duration_ms += duration_ms / 2U;
        if (!play_note(note == 'p' ? 0 : note_frequency(note, sharp, octave),
                       duration_ms, generation)) {
            return;
        }
        if (*text == ',') text++;
    }
}

static void player_task(void *arg) {
    (void)arg;
    if (bsp_audio_set_format(RTTTL_SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio format setup failed");
    } else {
        bsp_audio_set_volume(RTTTL_VOLUME);
    }

    rtttl_request_t request;
    for (;;) {
        if (xQueueReceive(s_queue, &request, portMAX_DELAY) == pdTRUE && request.song) {
            s_playing = true;
            play_song(request.song, request.generation);
            s_playing = false;
        }
    }
}

esp_err_t rtttl_player_start(void) {
    if (s_task) return ESP_OK;
    s_queue = xQueueCreate(4, sizeof(rtttl_request_t));
    if (!s_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(player_task, "rtttl", 3072, NULL, 4, &s_task) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void rtttl_player_stop(void) {
    s_generation++;
    if (s_queue) xQueueReset(s_queue);
}

esp_err_t rtttl_player_play(const char *song) {
    if (!s_queue || !song) return ESP_ERR_INVALID_STATE;
    /* A newer game effect is more useful than a delayed one. Cancel the
       current melody and discard queued effects before starting this song. */
    uint32_t generation = ++s_generation;
    xQueueReset(s_queue);
    rtttl_request_t request = { .song = song, .generation = generation };
    return xQueueSend(s_queue, &request, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool rtttl_player_is_playing(void) {
    return s_playing;
}
