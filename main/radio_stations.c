#include "radio_stations.h"

#include <string.h>

/* Preset progressive HTTP MP3 streams only (no HLS). Prefer clear-text HTTP
 * to keep TLS buffers off the no-PSRAM heap. Sources may geo-block or rotate;
 * the player surfaces a clear error when a stream fails. */
static const radio_station_t s_stations[] = {
    {
        .id = "cnr1",
        .name_zh = "中国之声",
        .name_en = "CNR Voice of China",
        .region_zh = "中国大陆",
        .category_zh = "新闻",
        .url = "http://lhttp.qingting.fm/live/15318317/64k.mp3",
    },
    {
        .id = "sh-news",
        .name_zh = "上海新闻广播",
        .name_en = "Shanghai News Radio",
        .region_zh = "中国大陆",
        .category_zh = "新闻",
        .url = "http://lhttp.qingting.fm/live/270/64k.mp3",
    },
    {
        .id = "sc-news",
        .name_zh = "四川综合广播",
        .name_en = "Sichuan News Radio",
        .region_zh = "中国大陆",
        .category_zh = "新闻",
        .url = "http://lhttp.qingting.fm/live/4906/64k.mp3",
    },
    {
        .id = "dl-sports",
        .name_zh = "大连体育广播",
        .name_en = "Dalian Sports Radio",
        .region_zh = "中国大陆",
        .category_zh = "体育",
        .url = "http://lhttp.qingting.fm/live/1085/64k.mp3",
    },
    {
        .id = "paradise",
        .name_zh = "放逐乐园",
        .name_en = "Radio Paradise",
        .region_zh = "全球",
        .category_zh = "音乐",
        .url = "http://stream.radioparadise.com/mp3-128",
    },
    {
        .id = "paradise-mellow",
        .name_zh = "放逐乐园轻柔",
        .name_en = "Radio Paradise Mellow",
        .region_zh = "全球",
        .category_zh = "音乐",
        .url = "http://stream.radioparadise.com/mellow-128",
    },
    {
        .id = "fip",
        .name_zh = "巴黎 FIP",
        .name_en = "FIP Paris",
        .region_zh = "法国",
        .category_zh = "音乐",
        .url = "http://icecast.radiofrance.fr/fip-midfi.mp3",
    },
    {
        .id = "bbc-ws",
        .name_zh = "BBC 世界服务",
        .name_en = "BBC World Service",
        .region_zh = "全球",
        .category_zh = "新闻",
        .url = "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service",
    },
    {
        .id = "classic-fm",
        .name_zh = "经典音乐 FM",
        .name_en = "Classic FM",
        .region_zh = "英国",
        .category_zh = "音乐",
        .url = "http://media-ice.musicradio.com/ClassicFMMP3",
    },
    {
        .id = "kexp",
        .name_zh = "KEXP 西雅图",
        .name_en = "KEXP Seattle",
        .region_zh = "美国",
        .category_zh = "音乐",
        .url = "http://kexp-mp3-128.streamguys1.com/kexp128.mp3",
    },
};

size_t radio_station_count(void)
{
    return sizeof(s_stations) / sizeof(s_stations[0]);
}

const radio_station_t *radio_station_at(size_t index)
{
    if (index >= radio_station_count()) {
        return NULL;
    }
    return &s_stations[index];
}

int radio_station_find_id(const char *id)
{
    if (!id) {
        return -1;
    }
    for (size_t i = 0; i < radio_station_count(); i++) {
        if (strcmp(s_stations[i].id, id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

size_t radio_station_next(size_t index)
{
    size_t n = radio_station_count();
    if (n == 0) {
        return 0;
    }
    return (index + 1) % n;
}

size_t radio_station_prev(size_t index)
{
    size_t n = radio_station_count();
    if (n == 0) {
        return 0;
    }
    return (index + n - 1) % n;
}
