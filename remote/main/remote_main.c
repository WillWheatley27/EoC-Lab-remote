// EoC Lab — ESP32-C3 BLE remote.
//
// Two buttons (ON/OFF, PAUSE), an SSD1306 OLED, internal RTC, and BLE
// advertising. The remote spends most of its life in deep sleep; either
// button wakes it via EXT1. After wake it shows AWAKE/READY, processes the
// originating press, and stays awake for 30 s of further user activity
// before clearing the OLED and going back to deep sleep.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "remote_ble.h"
#include "remote_buttons.h"
#include "remote_oled.h"
#include "remote_pins.h"
#include "remote_rtc.h"

static const char *TAG = "remote_main";

typedef enum {
    APP_IDLE = 0,
    APP_RECORDING,
    APP_PAUSED,
} app_state_t;

static const char *s_state_text(app_state_t s)
{
    switch (s) {
        case APP_IDLE:      return "AWAKE\nREADY";
        case APP_RECORDING: return "RECORDING";
        case APP_PAUSED:    return "PAUSED";
        default:            return "?";
    }
}

// Apply the soft state transition triggered by a button press. The remote
// has no feedback channel so it has to assume the main board obeys.
static app_state_t s_apply_press(app_state_t cur, remote_btn_id_t btn)
{
    if (btn == REMOTE_BTN_ONOFF) {
        return (cur == APP_IDLE) ? APP_RECORDING : APP_IDLE;
    }
    if (btn == REMOTE_BTN_PAUSE) {
        if (cur == APP_RECORDING) return APP_PAUSED;
        if (cur == APP_PAUSED)    return APP_RECORDING;
        // Pause while idle: no state change. Main board ignores TT=01 in idle.
    }
    return cur;
}

static uint8_t s_tt_for_btn(remote_btn_id_t btn)
{
    return (btn == REMOTE_BTN_ONOFF) ? REMOTE_BLE_TT_ONOFF : REMOTE_BLE_TT_PAUSE;
}

static void s_enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "30s idle reached — entering deep sleep");
    (void)remote_oled_clear();
    remote_oled_deinit();

    // Wake on either button going low. ESP32-C3 supports ANY_LOW (and
    // ANY_HIGH); ALL_LOW is an ESP32-original-only mode. ANY_LOW is what
    // we want anyway: either button independently triggers wake.
    esp_sleep_enable_ext1_wakeup(REMOTE_BTN_WAKE_MASK,
                                 ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
    // not reached
}

void app_main(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    remote_btn_id_t wake_btn = REMOTE_BTN_NONE;
    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t mask = esp_sleep_get_ext1_wakeup_status();
        wake_btn = remote_buttons_id_from_wake_mask(mask);
        ESP_LOGI(TAG, "Wake from EXT1 mask=0x%" PRIx64 " btn=%d", mask, (int)wake_btn);
    } else {
        ESP_LOGI(TAG, "Cold boot / non-EXT1 wake (cause=%d)", (int)cause);
    }

    remote_rtc_init();

    if (remote_oled_init() != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed; continuing without display");
    }

    if (remote_ble_init() != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed; aborting");
        // Show the failure for a short time so the user can see it, then sleep.
        (void)remote_oled_show("BLE FAIL");
        vTaskDelay(pdMS_TO_TICKS(2000));
        s_enter_deep_sleep();
    }

    QueueHandle_t btn_q = NULL;
    if (remote_buttons_init(&btn_q) != ESP_OK || btn_q == NULL) {
        ESP_LOGE(TAG, "Button init failed; aborting");
        s_enter_deep_sleep();
    }

    // If we woke from a real button press, synthesize that press as the first
    // event so the trigger goes out without the user having to press again.
    if (wake_btn != REMOTE_BTN_NONE) {
        remote_btn_event_t evt = {
            .id = wake_btn,
            .timestamp_us = esp_timer_get_time(),
        };
        xQueueSend(btn_q, &evt, 0);
    }

    app_state_t state = APP_IDLE;
    (void)remote_oled_show(s_state_text(state));

    int64_t last_activity_us = esp_timer_get_time();

    while (true) {
        int64_t now = esp_timer_get_time();
        int64_t idle_us = now - last_activity_us;
        int64_t remaining_ms = (int64_t)REMOTE_INACTIVITY_MS - (idle_us / 1000);
        if (remaining_ms <= 0) {
            s_enter_deep_sleep();
        }

        remote_btn_event_t evt;
        if (xQueueReceive(btn_q, &evt, pdMS_TO_TICKS(remaining_ms)) == pdTRUE) {
            ESP_LOGI(TAG, "Button %d pressed", (int)evt.id);
            app_state_t next = s_apply_press(state, evt.id);
            state = next;
            (void)remote_oled_show(s_state_text(state));

            // Advertise for 500 ms so the main board scanner has time to see
            // at least a few packets. This call blocks for the full window.
            (void)remote_ble_advertise_trigger(s_tt_for_btn(evt.id),
                                               REMOTE_ADV_WINDOW_MS);

            // Reset the inactivity timer AFTER advertising so the 500 ms window
            // doesn't eat into the user's 30 s budget.
            last_activity_us = esp_timer_get_time();
        }
        // Loop back; the queue timeout drives the sleep entry.
    }
}
