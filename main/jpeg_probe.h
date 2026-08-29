#pragma once

#include <stdint.h>

typedef enum {
    JPEG_PROBE_OK = 0,
    JPEG_PROBE_NOT_JPEG,
    JPEG_PROBE_PROGRESSIVE,
    JPEG_PROBE_UNSUPPORTED,
    JPEG_PROBE_TRUNCATED,
} jpeg_probe_t;

jpeg_probe_t jpeg_probe(const uint8_t *data, int length);
const char *jpeg_probe_str(jpeg_probe_t result);
