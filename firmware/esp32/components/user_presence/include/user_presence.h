#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Return codes mirrored in Rust (ctap2/user_presence.rs).
#define USER_PRESENCE_OK       0
#define USER_PRESENCE_DENIED   1
#define USER_PRESENCE_TIMEOUT  2
#define USER_PRESENCE_ERROR    3

// Blocks until phone approves/denies or timeout_ms elapses.
int user_presence_wait_for_approval(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
