#pragma once

#include <stdbool.h>
#include <stdint.h>

int jpeg_store_begin(uint32_t total_length);
int jpeg_store_write(const uint8_t *data, int length);
int jpeg_store_end(void);
void jpeg_store_clear(void);
bool jpeg_store_has_valid(void);
int jpeg_store_mmap(const uint8_t **data, int *length);
void jpeg_store_unmap(void);

int jpeg_frame_begin(uint32_t byte_count);
int jpeg_frame_write(uint32_t offset, const uint8_t *data, int length);
int jpeg_frame_mmap(const uint8_t **data, uint32_t byte_count);
void jpeg_frame_unmap(void);
uint32_t jpeg_frame_capacity(void);
