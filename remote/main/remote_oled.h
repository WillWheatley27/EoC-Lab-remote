#pragma once

#include "esp_err.h"

// Initialize the I2C bus and the SSD1306 controller. Safe to call once at boot
// and again after wake from deep sleep.
esp_err_t remote_oled_init(void);

// Render a multi-line text string. Lines are separated by '\n'. Up to 4 lines
// of ~21 characters fit on the 128x32 panel using the 5x7 font + 1px gap.
esp_err_t remote_oled_show(const char *text);

// Clear the panel (used right before deep sleep).
esp_err_t remote_oled_clear(void);

// Tear down the I2C peripheral cleanly. Call right before deep sleep so the
// SDA/SCL pins are released to the level set by external pullups.
void remote_oled_deinit(void);
