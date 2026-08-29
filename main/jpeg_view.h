#pragma once

#include <stdbool.h>

typedef void (*jpeg_view_result_cb_t)(bool success, void *user);

bool jpeg_view_init(jpeg_view_result_cb_t callback, void *user);
bool jpeg_view_request(void);
bool jpeg_view_is_active(void);
bool jpeg_view_is_busy(void);
void jpeg_view_exit(void);
