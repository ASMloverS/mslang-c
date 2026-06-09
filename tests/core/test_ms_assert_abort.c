// test_ms_assert_abort.c - verifies MS_ASSERT(0) aborts in Debug build (exit non-zero)
// Registered with WILL_FAIL TRUE: non-zero exit == test passes.
// Only built in Debug (NDEBUG not set), where the abort actually fires.
#include "mslang/ms_error.h"

int main(void) {
    MS_ASSERT(0); // must abort here in Debug; unreachable in Release (not compiled)
    return 0;     // unreachable in Debug
}
