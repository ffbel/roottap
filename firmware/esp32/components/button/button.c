#include "button.h"

static QueueHandle_t s_q;

void button_init(void) {
  if (!s_q)
    s_q = xQueueCreate(16, sizeof(button_event_t));
}

QueueHandle_t button_get_event_queue(void) { return s_q; }

bool button_publish(button_event_t ev) {
  if (!s_q)
    return false;
  return xQueueSend(s_q, &ev, 0) == pdTRUE;
}
