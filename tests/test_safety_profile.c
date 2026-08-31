#include "safety_profile.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    safety_profile_t profile;
    safety_profile_defaults(&profile);
    assert(profile.configured == 0);
    assert(profile.show_full_phone == 1);
    assert(!safety_profile_has_pin(&profile));

    strcpy(profile.phone, "13800138000");
    profile.show_full_phone = 0;
    safety_profile_seal(&profile);
    assert(safety_profile_is_valid(&profile));
    assert(profile.configured == 1);

    char shown[32];
    safety_profile_mask_phone(&profile, shown, sizeof(shown));
    assert(strcmp(shown, "13800138000") == 0);

    safety_profile_t tampered = profile;
    tampered.phone[0] = '9';
    assert(!safety_profile_is_valid(&tampered));

    profile.show_full_phone = 1;
    safety_profile_mask_phone(&profile, shown, sizeof(shown));
    assert(strcmp(shown, "13800138000") == 0);

    profile.pin_hash[0] = 1;
    assert(safety_profile_has_pin(&profile));
    return 0;
}
