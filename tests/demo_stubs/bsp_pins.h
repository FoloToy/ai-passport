#include "demo_test_stubs.h"

// The demo runtime tests only need the key-pad facts from the pin table; the real
// header pulls in SDK driver headers that the host stubs do not provide.
#define BSP_BTN_GPIO            0
#define BSP_BTN_MV_RELEASED_MIN 1900
