#include "jpeg_probe.h"

#include <stddef.h>

#define EOI_SEARCH_TAIL 64

static int is_unsupported_marker(uint8_t marker)
{
    switch (marker) {
        case 0xC3:
        case 0xC5: case 0xC6: case 0xC7:
        case 0xC9: case 0xCA: case 0xCB:
        case 0xCC:
        case 0xCD: case 0xCE: case 0xCF:
            return 1;
        default:
            return 0;
    }
}

jpeg_probe_t jpeg_probe(const uint8_t *data, int length)
{
    if (!data || length < 4 || data[0] != 0xFF || data[1] != 0xD8) {
        return JPEG_PROBE_NOT_JPEG;
    }

    int cursor = 2;
    int found_scan = 0;
    while (cursor + 1 < length) {
        if (data[cursor] != 0xFF) return JPEG_PROBE_NOT_JPEG;
        while (cursor < length && data[cursor] == 0xFF) cursor++;
        if (cursor >= length) return JPEG_PROBE_TRUNCATED;

        uint8_t marker = data[cursor++];
        if (marker == 0xD8 || marker == 0x01 ||
            (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (marker == 0xD9) break;
        if (marker == 0xDA) {
            found_scan = 1;
            break;
        }
        if (marker == 0xC2) return JPEG_PROBE_PROGRESSIVE;
        if (is_unsupported_marker(marker)) return JPEG_PROBE_UNSUPPORTED;
        if (cursor + 1 >= length) return JPEG_PROBE_TRUNCATED;
        int segment_length = ((int)data[cursor] << 8) | data[cursor + 1];
        if (segment_length < 2) return JPEG_PROBE_NOT_JPEG;
        cursor += segment_length;
    }
    if (!found_scan) return JPEG_PROBE_TRUNCATED;

    int tail = length - EOI_SEARCH_TAIL;
    if (tail < 2) tail = 2;
    for (int i = length - 2; i >= tail; --i) {
        if (data[i] == 0xFF && data[i + 1] == 0xD9) return JPEG_PROBE_OK;
    }
    return JPEG_PROBE_TRUNCATED;
}

const char *jpeg_probe_str(jpeg_probe_t result)
{
    switch (result) {
        case JPEG_PROBE_OK: return "baseline JPEG";
        case JPEG_PROBE_NOT_JPEG: return "invalid JPEG";
        case JPEG_PROBE_PROGRESSIVE: return "progressive JPEG is unsupported";
        case JPEG_PROBE_UNSUPPORTED: return "unsupported JPEG encoding";
        case JPEG_PROBE_TRUNCATED: return "truncated JPEG";
        default: return "unknown JPEG error";
    }
}
