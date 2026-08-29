#include "safety_profile.h"

#include <string.h>

#define SAFETY_MAGIC 0x31434653u /* SFC1 */

static uint32_t checksum_bytes(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t profile_checksum(const safety_profile_t *profile)
{
    safety_profile_t copy = *profile;
    copy.checksum = 0;
    return checksum_bytes((const uint8_t *)&copy, sizeof(copy));
}

void safety_profile_defaults(safety_profile_t *profile)
{
    if (!profile) return;
    memset(profile, 0, sizeof(*profile));
    profile->magic = SAFETY_MAGIC;
    profile->version = SAFETY_PROFILE_VERSION;
    profile->size = sizeof(*profile);
    profile->show_full_phone = 1;
    strncpy(profile->help_text, "您好，我可能迷路了，请帮我联系家人",
            sizeof(profile->help_text) - 1);
    strncpy(profile->wechat_note, "请添加我的家人，备注安心牌",
            sizeof(profile->wechat_note) - 1);
}

void safety_profile_seal(safety_profile_t *profile)
{
    if (!profile) return;
    profile->magic = SAFETY_MAGIC;
    profile->version = SAFETY_PROFILE_VERSION;
    profile->size = sizeof(*profile);
    profile->configured = 1;
    profile->checksum = profile_checksum(profile);
}

bool safety_profile_is_valid(const safety_profile_t *profile)
{
    if (!profile || profile->magic != SAFETY_MAGIC ||
        profile->version != SAFETY_PROFILE_VERSION ||
        profile->size != sizeof(*profile)) {
        return false;
    }
    return profile->checksum == profile_checksum(profile);
}

bool safety_profile_has_pin(const safety_profile_t *profile)
{
    if (!profile) return false;
    uint8_t value = 0;
    for (size_t i = 0; i < sizeof(profile->pin_hash); ++i) {
        value |= profile->pin_hash[i];
    }
    return value != 0;
}

void safety_profile_mask_phone(const safety_profile_t *profile,
                               char *output, size_t capacity)
{
    if (!output || capacity == 0) return;
    output[0] = '\0';
    if (!profile) return;

    size_t length = strnlen(profile->phone, sizeof(profile->phone));
    if (profile->show_full_phone || length < 7) {
        strncpy(output, profile->phone, capacity - 1);
        output[capacity - 1] = '\0';
        return;
    }

    size_t used = 0;
    for (size_t i = 0; i < length && used + 1 < capacity; ++i) {
        bool hidden = i >= 3 && i + 4 < length;
        output[used++] = hidden ? '*' : profile->phone[i];
    }
    output[used] = '\0';
}
