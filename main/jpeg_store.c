#include "jpeg_store.h"

#include "esp_partition.h"

#include <string.h>

#define HEADER_SIZE 0x1000u
#define DATA_OFFSET 0x1000u

static const char MAGIC[4] = {'J', 'P', 'G', '1'};
static uint32_t s_write_offset;
static esp_partition_mmap_handle_t s_source_map;
static esp_partition_mmap_handle_t s_frame_map;
static bool s_source_mapped;
static bool s_frame_mapped;

static const esp_partition_t *partition_named(const char *name)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_ANY, name);
}

static uint32_t stored_length(const esp_partition_t *partition)
{
    uint8_t header[8];
    if (!partition ||
        esp_partition_read(partition, 0, header, sizeof(header)) != ESP_OK ||
        memcmp(header, MAGIC, sizeof(MAGIC)) != 0) {
        return 0;
    }
    return (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
           ((uint32_t)header[6] << 16) | ((uint32_t)header[7] << 24);
}

int jpeg_store_begin(uint32_t total_length)
{
    const esp_partition_t *partition = partition_named("imgstore");
    if (!partition || total_length > partition->size - DATA_OFFSET) return -1;
    uint32_t data_erase = (total_length + 0xFFFu) & ~0xFFFu;
    if (esp_partition_erase_range(partition, 0,
                                  HEADER_SIZE + data_erase) != ESP_OK) {
        return -2;
    }
    s_write_offset = 0;
    return 0;
}

int jpeg_store_write(const uint8_t *data, int length)
{
    const esp_partition_t *partition = partition_named("imgstore");
    if (!partition || !data || length < 0 ||
        s_write_offset + (uint32_t)length > partition->size - DATA_OFFSET) {
        return -1;
    }
    if (esp_partition_write(partition, DATA_OFFSET + s_write_offset,
                            data, (size_t)length) != ESP_OK) {
        return -2;
    }
    s_write_offset += (uint32_t)length;
    return 0;
}

int jpeg_store_end(void)
{
    const esp_partition_t *partition = partition_named("imgstore");
    if (!partition || s_write_offset == 0) return -1;
    uint8_t header[8];
    memcpy(header, MAGIC, sizeof(MAGIC));
    header[4] = s_write_offset & 0xFF;
    header[5] = (s_write_offset >> 8) & 0xFF;
    header[6] = (s_write_offset >> 16) & 0xFF;
    header[7] = (s_write_offset >> 24) & 0xFF;
    return esp_partition_write(partition, 0, header, sizeof(header)) == ESP_OK ? 0 : -2;
}

void jpeg_store_clear(void)
{
    const esp_partition_t *partition = partition_named("imgstore");
    if (partition) (void)esp_partition_erase_range(partition, 0, HEADER_SIZE);
}

bool jpeg_store_has_valid(void)
{
    const esp_partition_t *partition = partition_named("imgstore");
    uint32_t length = stored_length(partition);
    return partition && length > 0 && length <= partition->size - DATA_OFFSET;
}

int jpeg_store_mmap(const uint8_t **data, int *length)
{
    const esp_partition_t *partition = partition_named("imgstore");
    uint32_t stored = stored_length(partition);
    if (!partition || !data || !length || stored == 0 ||
        stored > partition->size - DATA_OFFSET) {
        return -1;
    }
    if (s_source_mapped) jpeg_store_unmap();
    const void *mapped = NULL;
    if (esp_partition_mmap(partition, DATA_OFFSET, stored,
                           ESP_PARTITION_MMAP_DATA, &mapped,
                           &s_source_map) != ESP_OK) {
        return -2;
    }
    s_source_mapped = true;
    *data = mapped;
    *length = (int)stored;
    return 0;
}

void jpeg_store_unmap(void)
{
    if (!s_source_mapped) return;
    esp_partition_munmap(s_source_map);
    s_source_mapped = false;
}

int jpeg_frame_begin(uint32_t byte_count)
{
    const esp_partition_t *partition = partition_named("imgframe");
    if (!partition || byte_count > partition->size) return -1;
    uint32_t erase_length = (byte_count + 0xFFFu) & ~0xFFFu;
    return esp_partition_erase_range(partition, 0, erase_length) == ESP_OK ? 0 : -2;
}

int jpeg_frame_write(uint32_t offset, const uint8_t *data, int length)
{
    const esp_partition_t *partition = partition_named("imgframe");
    if (!partition || !data || length < 0 ||
        offset + (uint32_t)length > partition->size) {
        return -1;
    }
    return esp_partition_write(partition, offset, data, (size_t)length) == ESP_OK ? 0 : -2;
}

int jpeg_frame_mmap(const uint8_t **data, uint32_t byte_count)
{
    const esp_partition_t *partition = partition_named("imgframe");
    if (!partition || !data || byte_count > partition->size) return -1;
    if (s_frame_mapped) jpeg_frame_unmap();
    const void *mapped = NULL;
    if (esp_partition_mmap(partition, 0, byte_count,
                           ESP_PARTITION_MMAP_DATA, &mapped,
                           &s_frame_map) != ESP_OK) {
        return -2;
    }
    s_frame_mapped = true;
    *data = mapped;
    return 0;
}

void jpeg_frame_unmap(void)
{
    if (!s_frame_mapped) return;
    esp_partition_munmap(s_frame_map);
    s_frame_mapped = false;
}

uint32_t jpeg_frame_capacity(void)
{
    const esp_partition_t *partition = partition_named("imgframe");
    return partition ? partition->size : 0;
}
