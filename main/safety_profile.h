#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAFETY_PROFILE_VERSION 1u

// Keep this layout compatible with the original Senior Safety Card release so
// upgrading the application does not discard an already configured profile.
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t checksum;
    uint8_t configured;
    uint8_t show_full_address;
    uint8_t show_full_phone;
    uint8_t reserved;
    char name[40];
    char help_text[120];
    char home_area[72];
    char home_address[152];
    char contact_name[40];
    char relation[28];
    char phone[32];
    char backup_phone[32];
    char medical[224];
    char wechat_note[80];
    uint8_t pin_salt[16];
    uint8_t pin_hash[32];
} safety_profile_t;

void safety_profile_defaults(safety_profile_t *profile);
void safety_profile_seal(safety_profile_t *profile);
bool safety_profile_is_valid(const safety_profile_t *profile);
bool safety_profile_has_pin(const safety_profile_t *profile);
void safety_profile_mask_phone(const safety_profile_t *profile,
                               char *output, size_t capacity);
