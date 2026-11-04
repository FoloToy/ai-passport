#include "radio_stations.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(radio_station_count() >= 8);
    const radio_station_t *first = radio_station_at(0);
    assert(first);
    assert(first->name_zh && first->name_zh[0]);
    assert(strstr(first->url, "http") == first->url);
    assert(strstr(first->url, ".m3u8") == NULL);

    for (size_t i = 0; i < radio_station_count(); i++) {
        const radio_station_t *st = radio_station_at(i);
        assert(st);
        assert(st->id && st->id[0]);
        assert(st->url && strncmp(st->url, "http", 4) == 0);
        assert(strstr(st->url, ".m3u8") == NULL);
        assert(radio_station_find_id(st->id) == (int)i);
    }

    assert(radio_station_find_id("nope") < 0);
    assert(radio_station_next(radio_station_count() - 1) == 0);
    assert(radio_station_prev(0) == radio_station_count() - 1);
    assert(radio_station_at(radio_station_count()) == NULL);

    puts("test_radio_stations: PASS");
    return 0;
}
