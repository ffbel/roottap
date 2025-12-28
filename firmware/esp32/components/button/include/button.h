#pragma once
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BUTTON_EVENT_PRESSED = 1,
    BUTTON_EVENT_RELEASED = 2,
} button_event_type_t;

typedef struct {
    button_event_type_t type;
    int64_t timestamp_us;
} button_event_t;

// Backends will create + own this queue. App reads from it.
QueueHandle_t button_get_event_queue(void);

#ifdef __cplusplus
}
#endif
