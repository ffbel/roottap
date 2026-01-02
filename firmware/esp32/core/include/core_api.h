#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t core_ctx_size(void);

int core_init(
    uint8_t *ctx_mem,
    size_t ctx_mem_len
);

int core_handle_request(
    uint8_t *ctx_mem,
    size_t ctx_mem_len,
    const uint8_t *req,
    size_t req_len,
    uint8_t *resp,
    size_t resp_cap,
    size_t *out_resp_len
);

#ifdef __cplusplus
}
#endif
