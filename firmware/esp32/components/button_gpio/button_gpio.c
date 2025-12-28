//#include <stdint.h>
//#include "esp_err.h"
//#include "esp_timer.h"
//#include "driver/gpio.h"
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "freertos/queue.h"
//#include "button.h"

//#ifndef BUTTON_GPIO_NUM
//#define BUTTON_GPIO_NUM GPIO_NUM_0
//#endif

//#ifndef BUTTON_DEBOUNCE_MS
//#define BUTTON_DEBOUNCE_MS 30
//#endif

//static QueueHandle_t s_evt_q = NULL;
//static TaskHandle_t  s_task = NULL;

//QueueHandle_t button_get_event_queue(void) {
    //return s_evt_q;
//}

//static void IRAM_ATTR isr_handler(void *arg)
//{
    //BaseType_t hp = pdFALSE;
    //vTaskNotifyGiveFromISR(s_task, &hp);
    //if (hp) portYIELD_FROM_ISR();
//}

//static void button_task(void *arg)
//{
    //int last_level = 1; // pull-up => released
    //while (1) {
        //ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        //// debounce
        //vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));

        //int level = gpio_get_level(BUTTON_GPIO_NUM);
        //if (level != last_level) {
            //last_level = level;

            //button_event_t ev = {
                //.type = (level == 0) ? BUTTON_EVENT_PRESSED : BUTTON_EVENT_RELEASED,
                //.timestamp_us = esp_timer_get_time()
            //};
            //(void)xQueueSend(s_evt_q, &ev, 0);
        //}
    //}
//}

//esp_err_t button_gpio_init(void)
//{
    //if (s_evt_q) return ESP_OK;

    //s_evt_q = xQueueCreate(8, sizeof(button_event_t));
    //if (!s_evt_q) return ESP_ERR_NO_MEM;

    //// Task that handles debounce and event emission
    //xTaskCreate(button_task, "button_task", 2048, NULL, 10, &s_task);

    //gpio_config_t cfg = {
        //.pin_bit_mask = 1ULL << BUTTON_GPIO_NUM,
        //.mode = GPIO_MODE_INPUT,
        //.pull_up_en = GPIO_PULLUP_ENABLE,
        //.pull_down_en = GPIO_PULLDOWN_DISABLE,
        //.intr_type = GPIO_INTR_ANYEDGE, // press+release
    //};
    //ESP_ERROR_CHECK(gpio_config(&cfg));

    //ESP_ERROR_CHECK(gpio_install_isr_service(0));
    //ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO_NUM, isr_handler, NULL));

    //return ESP_OK;
//}
