#include "led.h"
#include "driver/gpio.h"

void led_init(led_t *led, gpio_num_t gpio, bool active_high)
{
    led->gpio = gpio;
    led->active_high = active_high;
    led->state = false;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    led_set(led, false);
}

void led_set(led_t *led, bool on)
{
    led->state = on;
    int level = on ? 1 : 0;
    if (!led->active_high) level = !level;
    gpio_set_level(led->gpio, level);
}

void led_toggle(led_t *led)
{
    led_set(led, !led->state);
}
