#include "ctaphid.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <string.h>

#include "core_api.h"   // your Rust FFI header

static const char *TAG = "ctaphid";

// ---- internal constants ----
#define INIT_PAYLOAD_MAX (CTAPHID_REPORT_LEN - 7)  // 57
#define CONT_PAYLOAD_MAX (CTAPHID_REPORT_LEN - 5)  // 59
#define MAX_MSG_SIZE     1024                      // tune later
#define MSG_TIMEOUT_US   (3 * 1000 * 1000ULL)      // 3s reassembly timeout

// CTAPHID error codes (payload for CTAPHID_ERROR)
#define ERR_INVALID_CMD   0x01
#define ERR_INVALID_PAR   0x02
#define ERR_INVALID_LEN   0x03
#define ERR_INVALID_SEQ   0x04
#define ERR_MSG_TIMEOUT   0x05
#define ERR_CHANNEL_BUSY  0x06

// ---- helpers ----
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}
static uint16_t be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
static void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

static void send_error(ctaphid_ctx_t *ctx, uint32_t cid, uint8_t err)
{
    uint8_t r[CTAPHID_REPORT_LEN] = {0};
    put_be32(r, cid);
    r[4] = (uint8_t)(CTAPHID_ERROR | 0x80);
    put_be16(&r[5], 1);
    r[7] = err;
    ctx->io.send_report(ctx->io.send_user, r);
}

// Thin wrapper; pacing is handled in usb_hid via queuing.
static int send_report_retry(ctaphid_ctx_t *ctx, const uint8_t *r)
{
    return ctx->io.send_report(ctx->io.send_user, r);
}

static void send_msg(ctaphid_ctx_t *ctx, uint32_t cid, uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint8_t r[CTAPHID_REPORT_LEN];
    memset(r, 0, sizeof(r));
    put_be32(r, cid);
    r[4] = (uint8_t)(cmd | 0x80);
    put_be16(&r[5], len);

    uint16_t n0 = len > INIT_PAYLOAD_MAX ? INIT_PAYLOAD_MAX : len;
    if (n0) memcpy(&r[7], payload, n0);
    int rc = send_report_retry(ctx, r);
    ESP_LOGI(TAG, "send_msg init cid=%08x cmd=%02x len=%u n0=%u rc=%d", (unsigned)cid, cmd, (unsigned)len, (unsigned)n0, rc);
    if (rc != 0) ESP_LOGW(TAG, "send_report init rc=%d", rc);

    uint16_t off = n0;
    uint8_t seq = 0;

    while (off < len) {
        memset(r, 0, sizeof(r));
        put_be32(r, cid);
        r[4] = seq; // continuation packet uses seq in byte4
        uint16_t n = (len - off) > CONT_PAYLOAD_MAX ? CONT_PAYLOAD_MAX : (len - off);
        memcpy(&r[5], payload + off, n);
        rc = send_report_retry(ctx, r);
        ESP_LOGI(TAG, "send_msg cont cid=%08x seq=%u n=%u rc=%d", (unsigned)cid, (unsigned)seq, (unsigned)n, rc);
        if (rc != 0) ESP_LOGW(TAG, "send_report cont rc=%d seq=%u", rc, (unsigned)seq);
        off += n;
        seq++;
    }
}

static void reset_reassembly(ctaphid_ctx_t *ctx)
{
    ctx->cur_cid = 0;
    ctx->cur_cmd = 0;
    ctx->cur_len = 0;
    ctx->got = 0;
    ctx->next_seq = 0;
    ctx->started_at_us = 0;
}

static uint32_t alloc_cid(void)
{
    uint32_t cid = 0;
    // avoid broadcast/zero; no persistence so collisions are still possible but unlikely
    do {
        cid = esp_random();
    } while (cid == 0 || cid == CTAPHID_BROADCAST_CID);
    return cid;
}

// ---- public API ----
void ctaphid_init(ctaphid_ctx_t *ctx, const ctaphid_io_t *io)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->io = *io;

    // init Rust core (placement)
    size_t need = core_ctx_size();
    if (need > sizeof(ctx->core_mem)) {
        ESP_LOGE(TAG, "core_ctx_size=%u too big for core_mem=%u", (unsigned)need, (unsigned)sizeof(ctx->core_mem));
    } else {
        int rc = core_init(ctx->core_mem, sizeof(ctx->core_mem));
        ESP_LOGI(TAG, "core_init rc=%d", rc);
    }
}

static void handle_complete_message(ctaphid_ctx_t *ctx)
{
    const uint8_t *msg = ctx->buf;
    size_t msg_len = ctx->cur_len;
    uint32_t cid = ctx->cur_cid;
    uint8_t cmd = ctx->cur_cmd;

    // reset reassembly state
    reset_reassembly(ctx);

    if (cmd == CTAPHID_PING) {
        send_msg(ctx, cid, CTAPHID_PING, msg, (uint16_t)msg_len);
        return;
    }

    if (cmd == CTAPHID_CBOR) {
        size_t out_len = 0;
        int rc = core_handle_request(
            ctx->core_mem, sizeof(ctx->core_mem),
            msg, msg_len,
            ctx->core_resp, sizeof(ctx->core_resp),
            &out_len
        );
        ESP_LOGI(TAG, "core_handle_request rc=%d out_len=%u", rc, (unsigned)out_len);


        if (rc != 0) {
            // For CTAP2 over CBOR, return 1-byte CTAP status in CBOR response payload.
            uint8_t err1[1] = {(uint8_t)rc};
            send_msg(ctx, cid, CTAPHID_CBOR, err1, 1);
            return;
        }
        send_msg(ctx, cid, CTAPHID_CBOR, ctx->core_resp, (uint16_t)out_len);
        return;
    }

    send_error(ctx, cid, ERR_INVALID_CMD);
}

void ctaphid_on_report(ctaphid_ctx_t *ctx, const uint8_t *report, size_t len)
{
    if (len != CTAPHID_REPORT_LEN) return;

    int64_t now_us = esp_timer_get_time();

    uint32_t cid = be32(report);
    uint8_t b4 = report[4];
    ESP_LOGI(TAG, "on_report cid=%08x b4=%02x len=%u", (unsigned)cid, b4, (unsigned)len);

    // timeout handling for in-flight message before processing new frame
    if (ctx->cur_len && ctx->started_at_us) {
        uint32_t expired_cid = ctx->cur_cid;
        if (now_us - (int64_t)ctx->started_at_us > (int64_t)MSG_TIMEOUT_US) {
            reset_reassembly(ctx);
            send_error(ctx, expired_cid, ERR_MSG_TIMEOUT);
            // drop stray continuation for timed-out transaction
            if ((b4 & 0x80) == 0 && cid == expired_cid) {
                return;
            }
        }
    }

    if (b4 & 0x80) {
        // INIT frame
        uint8_t cmd = (uint8_t)(b4 & 0x7F);
        uint16_t total = be16(&report[5]);
        const uint8_t *p = &report[7];
        uint16_t n = total > INIT_PAYLOAD_MAX ? INIT_PAYLOAD_MAX : total;

        if (total > MAX_MSG_SIZE) { send_error(ctx, cid, ERR_INVALID_LEN); return; }

        if (cmd == CTAPHID_CANCEL) {
            if (total != 0) { send_error(ctx, cid, ERR_INVALID_LEN); return; }
            if (ctx->cur_len && cid == ctx->cur_cid) {
                reset_reassembly(ctx);
            }
            return;
        }

        if (cmd == CTAPHID_INIT) {
            // INIT request payload is 8-byte nonce
            if (total != 8) { send_error(ctx, cid, ERR_INVALID_LEN); return; }
            if (cid != CTAPHID_BROADCAST_CID) { /* spec allows init on non-broadcast too */ }

            uint8_t resp[17] = {0};
            // resp: nonce(8) + newCID(4) + ver(1) + vMajor(1) + vMinor(1) + vBuild(1) + caps(1)
            memcpy(&resp[0], p, 8);
            uint32_t new_cid = alloc_cid();
            put_be32(&resp[8], new_cid);
            resp[12] = 2;   // CTAPHID protocol version
            resp[13] = 1;   // device major
            resp[14] = 0;   // minor
            resp[15] = 0;   // build
            resp[16] = 0x04; // capabilities: CBOR supported (bit2). Add others later.

            send_msg(ctx, cid, CTAPHID_INIT, resp, sizeof(resp));
            return;
        }

        if (ctx->cur_len != 0) {
            send_error(ctx, cid, ERR_CHANNEL_BUSY);
            return;
        }

        // start reassembly for PING/CBOR/etc
        ctx->cur_cid = cid;
        ctx->cur_cmd = cmd;
        ctx->cur_len = total;
        ctx->got = 0;
        ctx->next_seq = 0;
        ctx->started_at_us = (uint64_t)now_us;

        if (n) {
            memcpy(ctx->buf, p, n);
            ctx->got = n;
        }

        if (ctx->got >= ctx->cur_len) {
            handle_complete_message(ctx);
        }
        return;
    } else {
        // CONT frame
        uint8_t seq = b4;
        const uint8_t *p = &report[5];

        if (ctx->cur_len == 0) { send_error(ctx, cid, ERR_INVALID_SEQ); return; }
        if (cid != ctx->cur_cid) { send_error(ctx, cid, ERR_INVALID_SEQ); return; }
        if (seq != ctx->next_seq) { send_error(ctx, cid, ERR_INVALID_SEQ); return; }

        uint16_t remaining = (uint16_t)(ctx->cur_len - ctx->got);
        uint16_t n = remaining > CONT_PAYLOAD_MAX ? CONT_PAYLOAD_MAX : remaining;

        memcpy(ctx->buf + ctx->got, p, n);
        ctx->got += n;
        ctx->next_seq++;

        if (ctx->got >= ctx->cur_len) {
            handle_complete_message(ctx);
        }
    }
}
