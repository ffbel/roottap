#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum { EV_REQUEST=1, EV_APPROVE=2, EV_DENY=3 } event_type_t;

typedef struct { event_type_t type; } button_event_t;

void button_init(void);
QueueHandle_t button_get_event_queue(void);
bool button_publish(button_event_t ev);
