#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *id;
    const char *name_zh;
    const char *name_en;
    const char *region_zh;
    const char *category_zh;
    const char *url;
} radio_station_t;

size_t radio_station_count(void);
const radio_station_t *radio_station_at(size_t index);
int radio_station_find_id(const char *id);
size_t radio_station_next(size_t index);
size_t radio_station_prev(size_t index);
