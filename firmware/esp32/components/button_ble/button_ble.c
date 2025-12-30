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

#include "os/os_mbuf.h"
#include "host/ble_gatt.h"
#include "host/ble_hs_mbuf.h" 

#include "button.h"

static const char *TAG = "button_ble";

// Custom UUIDs (random example). You can regenerate later.
static const ble_uuid128_t UUID_SVC_UP = BLE_UUID128_INIT(
    0x5a,0x1c,0x2e,0x6f,0x8c,0x77,0x4b,0x6a,0x9e,0x2f,0x21,0xa0,0x9b,0x11,0x73,0xd1);

static const ble_uuid128_t UUID_CHR_CONFIRM = BLE_UUID128_INIT(
    0x5a,0x1c,0x2e,0x6f,0x8c,0x77,0x4b,0x6a,0x9e,0x2f,0x21,0xa0,0x9b,0x11,0x73,0xd2);

static const ble_uuid128_t UUID_CHR_REQUEST = BLE_UUID128_INIT(
    0x5a,0x1c,0x2e,0x6f,0x8c,0x77,0x4b,0x6a,0x9e,0x2f,0x21,0xa0,0x9b,0x11,0x73,0xd3);

static uint16_t g_confirm_handle;
static uint16_t g_request_handle = 0;
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint8_t g_last_request_value = 0;  // 0/1 just for reads
static QueueHandle_t s_evt_q;

esp_err_t button_ble_request_approval(void) {
    if (g_request_handle == 0) return ESP_ERR_INVALID_STATE;
    if (g_conn_handle == BLE_HS_CONN_HANDLE_NONE) return ESP_ERR_INVALID_STATE;

    uint8_t v = 1; // payload doesn't matter for now

    struct os_mbuf *om = ble_hs_mbuf_from_flat(&v, sizeof(v));
    if (!om) return ESP_ERR_NO_MEM;

    int rc = ble_gatts_notify_custom(g_conn_handle, g_request_handle, om);

    return rc == 0 ? ESP_OK : ESP_FAIL;
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

    button_publish((button_event_t){
        .type = (buf[0] == 1) ? EV_APPROVE : EV_DENY
    });

    return 0;
}

static int request_access_cb(uint16_t conn_handle,
                             uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    // We only support READ on this characteristic.
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Return 1 byte so the characteristic has a valid "value".
    return os_mbuf_append(ctxt->om, &g_last_request_value, sizeof(g_last_request_value)) == 0
           ? 0
           : BLE_ATT_ERR_INSUFFICIENT_RES;
}


static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC_UP.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &UUID_CHR_REQUEST.u,
                .access_cb = request_access_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &g_request_handle,
            },
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
                g_conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Connected (handle=%d)", g_conn_handle);
            } else {
                ESP_LOGW(TAG, "Connect failed; status=%d", event->connect.status);
                ble_app_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected");
            g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
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
    int rc = ble_gatts_start();
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_start rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "request_handle=%u", g_request_handle);

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
    ESP_LOGE(TAG, "count_cfg rc=%d", rc);
    if (rc != 0) return ESP_FAIL;

    rc = ble_gatts_add_svcs(gatt_svcs);
    ESP_LOGE(TAG, "add_svcs rc=%d", rc);
    // ESP_LOGI(TAG, "request_handle=%u", g_request_handle);
    if (rc != 0) return ESP_FAIL;

    ble_hs_cfg.sync_cb = ble_on_sync;

    // Start host
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "button_ble initialized");

    return ESP_OK;
}
