#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "button.h"
#include "button_gpio.h"
#include "button_ble.h"
#include "led.h"
#include "esp_log.h"

#include "core_api.h"
#include "usb_hid.h"
#include "ctaphid.h"
// #include "usb_cdc_cmd.h"

static const char *TAG = "main";

#define LED_GPIO GPIO_NUM_21   // adjust if LED uses a different pin

#include "nvs_flash.h"
#include "esp_err.h"

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }
}

static ctaphid_ctx_t s_ctap;

static int send_report(void *user, const uint8_t *r64) {
    (void)user;
    return usb_hid_send_report(r64, USB_HID_REPORT_LEN);
}

static void on_usb_out(void *user, const uint8_t *report, size_t len) {
    (void)user;
    ctaphid_on_report(&s_ctap, report, len);
}



void app_main(void)
{
    // IMPORTANT: don’t require BOOT during startup (GPIO0 is a strapping pin)
    vTaskDelay(pdMS_TO_TICKS(1500));
    init_nvs();

    ctaphid_io_t io = {
        .send_report = send_report,
        .send_user = NULL,
    };
    ctaphid_init(&s_ctap, &io);

                ESP_LOGI(TAG, "before hid");
    // ESP_ERROR_CHECK(usb_hid_init(on_usb_out, NULL));
    int rc = usb_hid_init(on_usb_out, NULL);
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "usb_hid_init rc=%d", rc);
    if (rc != 0) {
        ESP_LOGE(TAG, "usb_hid_init failed");
        return;
    }
    // usb_cdc_cmd_start();



    button_init();
    ESP_ERROR_CHECK(button_gpio_init());


    // led_t led;
    // led_init(&led, LED_GPIO, true);

    // vTaskDelay(pdMS_TO_TICKS(500));
    // ESP_ERROR_CHECK(button_ble_init());

    // QueueHandle_t q = button_get_event_queue();
    // button_event_t ev;

    // const TickType_t pending_timeout_ticks = pdMS_TO_TICKS(20000);  // timeout for awaiting approval
    // bool pending = false;
    // TickType_t pending_since = 0;


    // while (1) {
    //     TickType_t wait_ticks = portMAX_DELAY;
    //     if (pending) {
    //         TickType_t elapsed = xTaskGetTickCount() - pending_since;
    //         wait_ticks = (elapsed < pending_timeout_ticks)
    //                          ? (pending_timeout_ticks - elapsed)
    //                          : 0;
    //     }

    //     if (xQueueReceive(q, &ev, wait_ticks)) {

    //         if (ev.type == EV_REQUEST && !pending) {
    //             pending = true;
    //             pending_since = xTaskGetTickCount();
    //             ESP_LOGI(TAG, "Request -> notify phone");
    //             esp_err_t err = button_ble_request_approval();   // notify phone
    //             if (err != ESP_OK) {
    //                 pending = false;  // <-- critical: don’t get stuck
    //                 ESP_LOGI(TAG, "Request canceled: %s", esp_err_to_name(err));
    //                 // optional: blink LED / buzz to show “no phone”
    //             }
    //         }

    //         if (ev.type == EV_APPROVE && pending) {
    //             pending = false;
    //             led_toggle(&led);                // toggle ONLY here
    //         }

    //         if (ev.type == EV_DENY && pending) {
    //             pending = false;
    //             ESP_LOGI(TAG, "Request -> denied");
    //             // optional: indicate deny
    //         }
    //     } else if (pending) {
    //         pending = false;
    //         ESP_LOGI(TAG, "Request -> timeout, no response");
    //     }
    // }
}
