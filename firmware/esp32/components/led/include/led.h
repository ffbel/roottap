#pragma once
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t gpio;
    bool active_high;
    bool state;
} led_t;

void led_init(led_t *led, gpio_num_t gpio, bool active_high);
void led_set(led_t *led, bool on);
void led_toggle(led_t *led);

#ifdef __cplusplus
}
#endif
