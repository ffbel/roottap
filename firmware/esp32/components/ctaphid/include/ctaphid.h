#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTAPHID_REPORT_LEN 64
#define CTAPHID_BROADCAST_CID 0xFFFFFFFFu

// CTAPHID commands (unframed values)
#define CTAPHID_PING   0x01
#define CTAPHID_INIT   0x06
#define CTAPHID_CBOR   0x10
#define CTAPHID_CANCEL 0x11
#define CTAPHID_ERROR  0x3F

typedef int (*ctaphid_send_report_fn)(void *user, const uint8_t *report64);

typedef struct {
    // caller provides output function to send 64-byte IN reports (usb_hid_send_report wrapper)
    ctaphid_send_report_fn send_report;
    void *send_user;
} ctaphid_io_t;

// CTAP HID context (single in-flight message).
typedef struct ctaphid_ctx {
    ctaphid_io_t io;

    // Reassembly state
    uint32_t cur_cid;
    uint8_t  cur_cmd;
    uint16_t cur_len;
    uint16_t got;
    uint8_t  next_seq;
    uint8_t  buf[1024];   // MAX_MSG_SIZE in ctaphid.c

    // core workspace
    uint8_t core_mem[512];
    uint8_t core_resp[1024];
} ctaphid_ctx_t;

void ctaphid_init(ctaphid_ctx_t *ctx, const ctaphid_io_t *io);

// feed OUT report from host (exactly 64 bytes)
void ctaphid_on_report(ctaphid_ctx_t *ctx, const uint8_t *report, size_t len);

#ifdef __cplusplus
}
#endif
