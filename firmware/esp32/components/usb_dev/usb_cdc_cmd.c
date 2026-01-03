#include "usb_cdc_cmd.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

// TinyUSB includes (ESP-IDF)
#include "tusb.h"
#include "class/cdc/cdc_device.h"


static void reboot_to_rom_bootloader(void) {
    // On ESP32-S3, entering ROM download mode requires GPIO0 low during reset.
    // You need GPIO0 accessible and not hard-wired in a way that prevents this.

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GPIO_NUM_0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(GPIO_NUM_0, 0);  // force BOOT low
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_restart();
}

static bool line_eq(const char *line, const char *cmd) {
    // Trim trailing \r\n and spaces
    size_t n = strlen(line);
    while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ' || line[n-1] == '\t'))
        n--;
    // Trim leading spaces
    while (*line == ' ' || *line == '\t') line++;
    return (strlen(cmd) == n) && (strncmp(line, cmd, n) == 0);
}

void usb_cdc_cmd_task(void *arg) {
    (void)arg;

    // Make sure TinyUSB is initialized somewhere in your app:
    // tinyusb_driver_install(...) or tusb_init() depending on your ESP-IDF version/usage.

    static char linebuf[128];
    size_t idx = 0;

    while (1) {
        // TinyUSB device task (some setups require calling this periodically)
        tud_task();

        if (tud_cdc_connected() && tud_cdc_available()) {
            uint8_t ch;
            while (tud_cdc_available()) {
                tud_cdc_read(&ch, 1);

                if (ch == '\n' || ch == '\r') {
                    linebuf[idx] = 0;
                    if (idx > 0) {
                        // Optional: echo back
                        tud_cdc_write_str("OK\r\n");
                        tud_cdc_write_flush();

                        if (line_eq(linebuf, "reboot")) {
                            tud_cdc_write_str("Rebooting to ROM...\r\n");
                            tud_cdc_write_flush();
                            vTaskDelay(pdMS_TO_TICKS(30));
                            reboot_to_rom_bootloader();
                        }
                    }
                    idx = 0;
                } else {
                    if (idx < sizeof(linebuf) - 1) {
                        linebuf[idx++] = (char)ch;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void usb_cdc_cmd_start(void) {
    xTaskCreate(
        usb_cdc_cmd_task,
        "usb_cdc_cmd",
        4096,
        NULL,
        5,
        NULL
    );
}