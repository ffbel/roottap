#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "button.h"
#include "led.h"

// Provided by the button_gpio backend:
esp_err_t button_gpio_init(void);

#define LED_GPIO GPIO_NUM_21   // If your LED doesn’t work, we’ll adjust.

void app_main(void)
{
    // IMPORTANT: don’t require BOOT during startup (GPIO0 is a strapping pin)
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_ERROR_CHECK(button_gpio_init());

    led_t led;
    led_init(&led, LED_GPIO, /*active_high=*/true);

    QueueHandle_t q = button_get_event_queue();
    button_event_t ev;

    while (1) {
        if (xQueueReceive(q, &ev, portMAX_DELAY)) {
            if (ev.type == BUTTON_EVENT_PRESSED) {
                led_toggle(&led);
                printf("toggle LED\n");
            }
        }
    }
}
