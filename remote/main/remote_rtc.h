#pragma once

#include <stdbool.h>
#include <stdint.h>

// On a cold boot (POR), seed the C3 RTC from the firmware build epoch encoded
// in __DATE__ / __TIME__ at compile time. On any other reset reason (including
// wake-from-deep-sleep) this is a no-op so the RTC keeps counting through
// sleep.
//
// The C3 RTC keeps running on the slow internal RC oscillator across deep
// sleep, but loses time on power-cycle / re-flash. After re-flashing, the
// initial wall-clock will jump to whatever the build epoch was; use
// remote_rtc_set_from_string() to set a different time at runtime.
void remote_rtc_init(void);

// Set the RTC from a 12-character BCD-style decimal string YYMMDDHHMMSS.
// Returns false on parse error.
bool remote_rtc_set_from_string(const char *yymmddhhmmss);

// Fill a 6-byte BCD buffer with the current YY/MM/DD/HH/MM/SS in big-endian
// pair order (out[0]=YY, out[5]=SS). YY is computed as (year - 2000) and is
// clamped to 0..99.
void remote_rtc_now_bcd6(uint8_t out[6]);
