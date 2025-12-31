#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "button.h"
#include "button_gpio.h"
#include "button_ble.h"
#include "led.h"
#include "esp_log.h"

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



void app_main(void)
{
    // IMPORTANT: don’t require BOOT during startup (GPIO0 is a strapping pin)
    vTaskDelay(pdMS_TO_TICKS(1500));
    init_nvs();

    button_init();
    ESP_ERROR_CHECK(button_gpio_init());


    led_t led;
    led_init(&led, LED_GPIO, true);

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_ERROR_CHECK(button_ble_init());

    QueueHandle_t q = button_get_event_queue();
    button_event_t ev;

    bool pending = false;

    while (1) {
        if (xQueueReceive(q, &ev, portMAX_DELAY)) {

            if (ev.type == EV_REQUEST && !pending) {
                pending = true;
                ESP_LOGI(TAG, "Request -> notify phone");
                button_ble_request_approval();   // 🔔 notify phone
            }

            if (ev.type == EV_APPROVE && pending) {
                pending = false;
                led_toggle(&led);                // ✅ toggle ONLY here
            }

            if (ev.type == EV_DENY && pending) {
                pending = false;
                ESP_LOGI(TAG, "Request -> denied");
                // optional: indicate deny
            }
        }
    }
}
