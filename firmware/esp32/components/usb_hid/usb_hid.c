// USB HID (CTAP) glue on top of TinyUSB.

#include "usb_hid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tinyusb.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

static const char *TAG = "usb_hid";

static usb_hid_out_cb_t s_out_cb = NULL;
static void *s_out_user = NULL;
static volatile bool s_in_busy = false; // true while an IN transfer is in flight

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

enum { ITF_CDC_0 = 0, ITF_CDC_0_DATA, ITF_HID, ITF_TOTAL };
#define EPNUM_CDC_0_NOTIF 0x81
#define EPNUM_CDC_0_OUT   0x02
#define EPNUM_CDC_0_IN    0x82
#define EPNUM_HID_OUT     0x03
#define EPNUM_HID_IN      0x83
#define HID_POLL_INTERVAL_MS 1

static const uint8_t s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_TOTAL, 0,
        TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_INOUT_DESC_LEN,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(ITF_CDC_0, 0, EPNUM_CDC_0_NOTIF, 8,
                       EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
    TUD_HID_INOUT_DESCRIPTOR(ITF_HID, 0, HID_ITF_PROTOCOL_NONE,
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

// Called when a report has completed sending (IN endpoint is free again).
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const *report, uint16_t len)
{
    (void)itf;
    (void)report;
    (void)len;
    s_in_busy = false;
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

    const tinyusb_config_cdcacm_t cdc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 256,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL,
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&cdc_cfg));
    ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0));  // routes stdin/stdout/ESP_LOG to CDC

    return 0;
}

int usb_hid_send_report(const uint8_t *report, size_t len)
{
    if (len != USB_HID_REPORT_LEN) {
        return -1;
    }
    // Wait for any in-flight IN transfer to finish.
    for (int tries = 0; tries < 200 && s_in_busy; tries++) { // ~200ms max
        tud_task(); // pump TinyUSB state so completions fire
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (s_in_busy) return -2;
    s_in_busy = true;
    for (int tries = 0; tries < 200; tries++) {
        if (tud_hid_ready()) break;
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!tud_hid_ready()) { s_in_busy = false; return -2; }

    const bool ok = tud_hid_report(0, report, (uint16_t)len);
    if (!ok) {
        s_in_busy = false;
        return -3;
    }
    return 0;
}
