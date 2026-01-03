// USB HID (CTAP) glue on top of TinyUSB.

#include "usb_hid.h"
#include "esp_log.h"

#include "tinyusb.h"
#include "tusb.h"
#include "class/hid/hid_device.h"

static const char *TAG = "usb_hid";

static usb_hid_out_cb_t s_out_cb = NULL;
static void *s_out_user = NULL;

// FIDO/U2F HID report descriptor (64-byte IN/OUT).
static const uint8_t s_hid_report_desc[] = {
    0x06, 0xD0, 0xF1,         // Usage Page (FIDO Alliance)
    0x09, 0x01,               // Usage (U2F HID Authenticator Device)
    0xA1, 0x01,               // Collection (Application)
    0x09, 0x20,               //   Usage (Input Report Data)
    0x15, 0x00,               //   Logical Min (0)
    0x26, 0xFF, 0x00,         //   Logical Max (255)
    0x75, 0x08,               //   Report Size (8)
    0x95, USB_HID_REPORT_LEN, //   Report Count (64)
    0x81, 0x02,               //   Input (Data,Var,Abs)
    0x09, 0x21,               //   Usage (Output Report Data)
    0x95, USB_HID_REPORT_LEN, //   Report Count (64)
    0x91, 0x02,               //   Output (Data,Var,Abs)
    0xC0                      // End Collection
};

enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_HID_OUT 0x01
#define EPNUM_HID_IN  0x81
#define HID_POLL_INTERVAL_MS 1

static const uint8_t s_configuration_descriptor[] = {
    // Configuration descriptor + single HID interface (IN/OUT, 64-byte EPs).
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0,
                          TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                             sizeof(s_hid_report_desc),
                             EPNUM_HID_OUT, EPNUM_HID_IN,
                             USB_HID_REPORT_LEN, HID_POLL_INTERVAL_MS),
};

// TinyUSB calls this to get the report descriptor for interface itf.
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return s_hid_report_desc;
}

// Control GET_REPORT not used; STALL by returning zero length.
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// Called by TinyUSB stack when host sends OUT report.
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)itf;
    (void)report_id;
    (void)report_type;

    if (bufsize != USB_HID_REPORT_LEN) {
        ESP_LOGW(TAG, "OUT report len=%u (expected %u)", bufsize, USB_HID_REPORT_LEN);
    }
    if (s_out_cb) {
        s_out_cb(s_out_user, buffer, bufsize);
    }
}

int usb_hid_init(usb_hid_out_cb_t cb, void *user)
{
    s_out_cb = cb;
    s_out_user = user;

    const tinyusb_config_t cfg = {
        .device_descriptor = NULL,         // use esp_tinyusb defaults
        .string_descriptor = NULL,         // use default strings
        .string_descriptor_count = 0,
        .external_phy = false,
        .configuration_descriptor = s_configuration_descriptor,
        .self_powered = false,
        .vbus_monitor_io = -1,
    };

    esp_err_t err = tinyusb_driver_install(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(err));
        return -1;
    }

    ESP_LOGI(TAG, "USB HID initialized (FIDO report len=%d)", USB_HID_REPORT_LEN);
    return 0;
}

int usb_hid_send_report(const uint8_t *report, size_t len)
{
    if (len != USB_HID_REPORT_LEN) {
        return -1;
    }
    if (!tud_hid_ready()) {
        return -2;
    }

    const bool ok = tud_hid_report(0, report, (uint16_t)len);
    return ok ? 0 : -3;
}
