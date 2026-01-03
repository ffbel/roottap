#include "usb_cdc_cmd.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
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

static const char *TAG = "usb_cdc_cmd";

typedef struct {
    bool active;
    size_t expected;
    size_t received;
    esp_ota_handle_t handle;
    const esp_partition_t *part;
} ota_state_t;

static bool line_eq(const char *line, const char *cmd) {
    // Trim trailing \r\n and spaces
    size_t n = strlen(line);
    while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ' || line[n-1] == '\t'))
        n--;
    // Trim leading spaces
    while (*line == ' ' || *line == '\t') line++;
    return (strlen(cmd) == n) && (strncmp(line, cmd, n) == 0);
}

static void ota_reset(ota_state_t *ota, bool abort_write) {
    if (ota->active) {
        if (abort_write) {
            esp_ota_abort(ota->handle);
        }
    }
    memset(ota, 0, sizeof(*ota));
}

static bool ota_begin(ota_state_t *ota, size_t size) {
    ota_reset(ota, false);
    ota->part = esp_ota_get_next_update_partition(NULL);
    if (!ota->part) {
        ESP_LOGE(TAG, "No OTA partition");
        return false;
    }
    esp_err_t err = esp_ota_begin(ota->part, size, &ota->handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota begin failed: %s", esp_err_to_name(err));
        return false;
    }
    ota->expected = size;
    ota->received = 0;
    ota->active = true;
    return true;
}

void usb_cdc_cmd_task(void *arg) {
    (void)arg;

    // Make sure TinyUSB is initialized somewhere in your app:
    // tinyusb_driver_install(...) or tusb_init() depending on your ESP-IDF version/usage.

    static char linebuf[128];
    size_t idx = 0;
    static ota_state_t ota;

    while (1) {
        // TinyUSB device task (some setups require calling this periodically)
        tud_task();

        if (tud_cdc_connected() && tud_cdc_available()) {
            // If OTA is active, read raw bytes directly
            if (ota.active) {
                uint8_t buf[512];
                size_t to_read = ota.expected - ota.received;
                if (to_read > sizeof(buf)) to_read = sizeof(buf);
                size_t n = tud_cdc_read(buf, to_read);
                if (n > 0) {
                    esp_err_t err = esp_ota_write(ota.handle, buf, n);
                    if (err != ESP_OK) {
                        tud_cdc_write_str("OTA ERR write\r\n");
                        tud_cdc_write_flush();
                        ota_reset(&ota, true);
                        continue;
                    }
                    ota.received += n;
                    if (ota.received >= ota.expected) {
                        esp_err_t end_err = esp_ota_end(ota.handle);
                        if (end_err == ESP_OK) {
                            end_err = esp_ota_set_boot_partition(ota.part);
                        }
                        if (end_err == ESP_OK) {
                            tud_cdc_write_str("OTA OK\r\n");
                            tud_cdc_write_flush();
                            vTaskDelay(pdMS_TO_TICKS(50));
                            esp_restart();
                        } else {
                            tud_cdc_write_str("OTA ERR end\r\n");
                            tud_cdc_write_flush();
                        }
                        ota_reset(&ota, false);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(1));
                continue;
            }

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
                        } else if (strncmp(linebuf, "ota ", 4) == 0) {
                            size_t sz = strtoul(linebuf + 4, NULL, 10);
                            if (sz == 0) {
                                tud_cdc_write_str("OTA ERR size\r\n");
                            } else if (ota_begin(&ota, sz)) {
                                tud_cdc_write_str("OTA BEGIN\r\n");
                            } else {
                                tud_cdc_write_str("OTA ERR begin\r\n");
                            }
                            tud_cdc_write_flush();
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
