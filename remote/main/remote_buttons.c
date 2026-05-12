#include "remote_buttons.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "remote_pins.h"

static const char *TAG = "buttons";
static QueueHandle_t s_queue;

// Polled GPIO debouncer. We deliberately do not use GPIO interrupts: in deep
// sleep wake comes from EXT1 (level-triggered on these pins), and while awake
// a 10 ms poll with a 50 ms confirm is plenty fast and matches the main board
// button.c behaviour.
static void s_button_task(void *arg)
{
    (void)arg;
    bool last_onoff = (gpio_get_level(REMOTE_BTN_ONOFF_GPIO) != 0);
    bool last_pause = (gpio_get_level(REMOTE_BTN_PAUSE_GPIO) != 0);

    while (true) {
        bool onoff = (gpio_get_level(REMOTE_BTN_ONOFF_GPIO) != 0);
        bool pause = (gpio_get_level(REMOTE_BTN_PAUSE_GPIO) != 0);

        if (onoff != last_onoff || pause != last_pause) {
            // Confirm debounce: re-sample after REMOTE_BTN_DEBOUNCE_MS.
            vTaskDelay(pdMS_TO_TICKS(REMOTE_BTN_DEBOUNCE_MS));
            onoff = (gpio_get_level(REMOTE_BTN_ONOFF_GPIO) != 0);
            pause = (gpio_get_level(REMOTE_BTN_PAUSE_GPIO) != 0);

            // Falling edge (active low) on ON/OFF.
            if (last_onoff && !onoff) {
                remote_btn_event_t evt = {
                    .id = REMOTE_BTN_ONOFF,
                    .timestamp_us = esp_timer_get_time(),
                };
                if (s_queue) {
                    xQueueSend(s_queue, &evt, 0);
                }
            }
            // Falling edge on PAUSE.
            if (last_pause && !pause) {
                remote_btn_event_t evt = {
                    .id = REMOTE_BTN_PAUSE,
                    .timestamp_us = esp_timer_get_time(),
                };
                if (s_queue) {
                    xQueueSend(s_queue, &evt, 0);
                }
            }
            last_onoff = onoff;
            last_pause = pause;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t remote_buttons_init(QueueHandle_t *out_queue)
{
    gpio_config_t cfg = {
        .pin_bit_mask = REMOTE_BTN_WAKE_MASK,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    // The queue must already exist when the task starts so a synthetic wake
    // event (posted by the caller before this function) is preserved. We
    // create it here only if the caller did not pre-create one via the
    // out parameter.
    if (s_queue == NULL) {
        s_queue = xQueueCreate(8, sizeof(remote_btn_event_t));
        if (s_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (out_queue) {
        *out_queue = s_queue;
    }

    if (xTaskCreate(s_button_task, "btn_task", 3072, NULL, 8, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

remote_btn_id_t remote_buttons_id_from_wake_mask(uint64_t wake_mask)
{
    if (wake_mask & (1ULL << REMOTE_BTN_ONOFF_GPIO)) {
        return REMOTE_BTN_ONOFF;
    }
    if (wake_mask & (1ULL << REMOTE_BTN_PAUSE_GPIO)) {
        return REMOTE_BTN_PAUSE;
    }
    return REMOTE_BTN_NONE;
}
