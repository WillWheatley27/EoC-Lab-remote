#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    REMOTE_BTN_NONE = 0,
    REMOTE_BTN_ONOFF,
    REMOTE_BTN_PAUSE,
} remote_btn_id_t;

typedef struct {
    remote_btn_id_t id;
    int64_t timestamp_us;   // esp_timer_get_time() at the moment of press
} remote_btn_event_t;

// Configure both buttons as inputs with internal pullups. Spawns a polling task
// that posts debounced press events (level high->low transition) to the queue.
// The queue is created internally and returned via *out_queue. The caller may
// pre-post a synthetic wake event on the queue before calling this function.
esp_err_t remote_buttons_init(QueueHandle_t *out_queue);

// Inspect a wake-from-deep-sleep status bitmap (from
// esp_sleep_get_ext1_wakeup_status()) and return the corresponding button id,
// or REMOTE_BTN_NONE if no recognised button bit is set.
remote_btn_id_t remote_buttons_id_from_wake_mask(uint64_t wake_mask);
