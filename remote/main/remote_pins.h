#pragma once

// All hardware pin assignments for the ESP32-C3 remote live here.
// Change a value in this file and rebuild to remap.
//
// GPIO 2, 8, 9 are strapping pins on the ESP32-C3 and are reserved.

#include "driver/gpio.h"
#include "driver/i2c.h"

// Buttons (active low, internal pullup, both wake from deep sleep).
#define REMOTE_BTN_ONOFF_GPIO       GPIO_NUM_4
#define REMOTE_BTN_PAUSE_GPIO       GPIO_NUM_5
#define REMOTE_BTN_WAKE_MASK        ((1ULL << REMOTE_BTN_ONOFF_GPIO) | \
                                     (1ULL << REMOTE_BTN_PAUSE_GPIO))
#define REMOTE_BTN_DEBOUNCE_MS      50

// SSD1306 OLED on a dedicated I2C bus.
#define REMOTE_OLED_SDA_GPIO        GPIO_NUM_6
#define REMOTE_OLED_SCL_GPIO        GPIO_NUM_7
#define REMOTE_OLED_I2C_PORT        I2C_NUM_0
#define REMOTE_OLED_I2C_HZ          100000
#define REMOTE_OLED_I2C_ADDR        0x3C

// Sleep / advertising timing.
#define REMOTE_INACTIVITY_MS        30000   // sleep after 30s idle
#define REMOTE_ADV_WINDOW_MS        500     // advertise for 500ms per press

// Sanity check: refuse to compile if any reserved strapping pin slips in.
#if (REMOTE_BTN_ONOFF_GPIO == 2)  || (REMOTE_BTN_PAUSE_GPIO == 2)  || \
    (REMOTE_OLED_SDA_GPIO  == 2)  || (REMOTE_OLED_SCL_GPIO  == 2)  || \
    (REMOTE_BTN_ONOFF_GPIO == 8)  || (REMOTE_BTN_PAUSE_GPIO == 8)  || \
    (REMOTE_OLED_SDA_GPIO  == 8)  || (REMOTE_OLED_SCL_GPIO  == 8)  || \
    (REMOTE_BTN_ONOFF_GPIO == 9)  || (REMOTE_BTN_PAUSE_GPIO == 9)  || \
    (REMOTE_OLED_SDA_GPIO  == 9)  || (REMOTE_OLED_SCL_GPIO  == 9)
#error "remote_pins.h uses an ESP32-C3 strapping pin (GPIO2/8/9). Pick another."
#endif
