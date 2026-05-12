#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// TT byte values that go into the trigger UUID. Must match the parser in
// the main board's main/ble_trigger.c.
#define REMOTE_BLE_TT_PAUSE   0x01   // pause toggle
#define REMOTE_BLE_TT_ONOFF   0x02   // record start/stop toggle

// Initialize NVS (BLE controller storage) and bring up the NimBLE host stack.
// Returns once the host is synced and ready to advertise.
esp_err_t remote_ble_init(void);

// Build the trigger UUID for the given TT byte and the current RTC value,
// then advertise it as a NONCONN_IND service-UUID payload for window_ms,
// and stop. Blocks for window_ms.
//
// UUID big-endian layout matches the main board parser:
//   [0..7]   = 0x00
//   [8]      = tt_byte (0x01 or 0x02)
//   [9]      = 0x00
//   [10..15] = BCD YY/MM/DD/HH/MM/SS from remote_rtc_now_bcd6()
esp_err_t remote_ble_advertise_trigger(uint8_t tt_byte, uint32_t window_ms);
