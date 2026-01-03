#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_HID_REPORT_LEN 64

typedef void (*usb_hid_out_cb_t)(void *user, const uint8_t *report, size_t len);

/**
 * Initialize USB HID (FIDO/U2F usage) and register callback for OUT reports.
 * OUT reports are exactly USB_HID_REPORT_LEN bytes (64) for CTAPHID.
 */
int usb_hid_init(usb_hid_out_cb_t cb, void *user);

/** Send one IN report to host (must be 64 bytes). */
int usb_hid_send_report(const uint8_t *report, size_t len);

#ifdef __cplusplus
}
#endif
