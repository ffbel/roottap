#include "user_presence.h"

#include <stdbool.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "button.h"
#include "button_ble.h"

static const char *TAG = "user_presence";

int user_presence_wait_for_approval(uint32_t timeout_ms)
{
    // Ensure the shared event queue exists before we block on it.
    button_init();
    QueueHandle_t q = button_get_event_queue();
    if (!q) {
        ESP_LOGE(TAG, "button queue not ready");
        return USER_PRESENCE_ERROR;
    }

    // Drop any stale events so we only react to this request.
    button_event_t ev;
    while (xQueueReceive(q, &ev, 0) == pdTRUE) { }

    TickType_t now = xTaskGetTickCount();
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    TickType_t next_retry = now;
    bool request_sent = false;
    bool logged_failure = false;

    while (1) {
        now = xTaskGetTickCount();

        if (!request_sent && now >= next_retry) {
            esp_err_t err = button_ble_request_approval();
            if (err == ESP_OK) {
                request_sent = true;
            } else if (!logged_failure) {
                ESP_LOGW(TAG, "BLE request failed: %s", esp_err_to_name(err));
                logged_failure = true;
            }
            next_retry = now + pdMS_TO_TICKS(500);
        }

        TickType_t wait = (deadline > now) ? (deadline - now) : 0;
        if (!request_sent) {
            TickType_t until_retry = (next_retry > now) ? (next_retry - now) : 0;
            if (until_retry < wait) {
                wait = until_retry;
            }
        }

        if (xQueueReceive(q, &ev, wait) == pdTRUE) {
            if (ev.type == EV_APPROVE) {
                return USER_PRESENCE_OK;
            }
            if (ev.type == EV_DENY) {
                return USER_PRESENCE_DENIED;
            }
            // Ignore other events (e.g. EV_REQUEST from GPIO).
        } else if (xTaskGetTickCount() >= deadline) {
            // Timed out waiting for a reply.
            return USER_PRESENCE_TIMEOUT;
        }
    }
}
