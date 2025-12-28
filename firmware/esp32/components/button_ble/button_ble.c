#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "button.h"

static const char *TAG = "button_ble";

// Custom UUIDs (random example). You can regenerate later.
static const ble_uuid128_t UUID_SVC_UP = BLE_UUID128_INIT(
    0x5a,0x1c,0x2e,0x6f,0x8c,0x77,0x4b,0x6a,0x9e,0x2f,0x21,0xa0,0x9b,0x11,0x73,0xd1);

static const ble_uuid128_t UUID_CHR_CONFIRM = BLE_UUID128_INIT(
    0x5a,0x1c,0x2e,0x6f,0x8c,0x77,0x4b,0x6a,0x9e,0x2f,0x21,0xa0,0x9b,0x11,0x73,0xd2);

static uint16_t g_confirm_handle;
static QueueHandle_t s_evt_q;

QueueHandle_t button_get_event_queue(void) {
    return s_evt_q;
}

// ---- GATT callback: phone writes "confirm" here
static int confirm_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Read payload (optional). For now we ignore contents.
    // You can enforce a token/challenge later.
    uint8_t buf[64];
    int len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > (int)sizeof(buf)) len = sizeof(buf);
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, NULL);

    ESP_LOGI(TAG, "BLE confirm write, len=%d", len);

    button_event_t ev = {.type = BUTTON_EVENT_PRESSED};
    (void)xQueueSend(s_evt_q, &ev, 0);

    return 0;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC_UP.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &UUID_CHR_CONFIRM.u,
                .access_cb = confirm_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                .val_handle = &g_confirm_handle,
            },
            {0}
        },
    },
    {0}
};

static void ble_app_advertise(void);

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected");
            } else {
                ESP_LOGW(TAG, "Connect failed; status=%d", event->connect.status);
                ble_app_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected");
            ble_app_advertise();
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ble_app_advertise();
            return 0;

        default:
            return 0;
    }
}

static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    // Advertise name + service UUID
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids128 = (ble_uuid128_t*)&UUID_SVC_UP;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising...");
    }
}

static void ble_on_sync(void)
{
    // Ensure we have an address
    ble_app_advertise();
}

static void host_task(void *param)
{
    nimble_port_run(); // blocks until nimble_port_stop()
    nimble_port_freertos_deinit();
}

esp_err_t button_ble_init(void)
{
    if (s_evt_q) return ESP_OK;
    s_evt_q = xQueueCreate(8, sizeof(button_event_t));
    if (!s_evt_q) return ESP_ERR_NO_MEM;

    // Init NimBLE
    nimble_port_init();

    // GAP/GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set device name
    ble_svc_gap_device_name_set("roottap-up");

    // Add our service
    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) return ESP_FAIL;

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) return ESP_FAIL;

    ble_hs_cfg.sync_cb = ble_on_sync;

    // Start host
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "button_ble initialized");

    return ESP_OK;
}
