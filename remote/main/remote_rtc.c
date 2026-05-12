#include "remote_rtc.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "rtc";

// Convert __DATE__ ("Mmm dd yyyy") to a (mon, day, year) triple.
static bool s_parse_build_date(int *mon, int *day, int *year)
{
    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char m[4] = {0};
    int d = 0, y = 0;
    if (sscanf(__DATE__, "%3s %d %d", m, &d, &y) != 3) {
        return false;
    }
    const char *p = strstr(months, m);
    if (p == NULL) {
        return false;
    }
    *mon = (int)((p - months) / 3) + 1;
    *day = d;
    *year = y;
    return true;
}

// Convert __TIME__ ("hh:mm:ss") to a (hour, minute, second) triple.
static bool s_parse_build_time(int *hour, int *minute, int *second)
{
    int h, m, s;
    if (sscanf(__TIME__, "%d:%d:%d", &h, &m, &s) != 3) {
        return false;
    }
    *hour = h;
    *minute = m;
    *second = s;
    return true;
}

void remote_rtc_init(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason != ESP_RST_POWERON) {
        ESP_LOGI(TAG, "Reset reason %d — keeping existing RTC", (int)reason);
        return;
    }

    int mon, day, year, hour, minute, second;
    if (!s_parse_build_date(&mon, &day, &year) ||
        !s_parse_build_time(&hour, &minute, &second)) {
        ESP_LOGW(TAG, "Could not parse build epoch; RTC left at 1970");
        return;
    }

    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = 0;
    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        ESP_LOGW(TAG, "mktime failed");
        return;
    }
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "RTC seeded from build epoch %04d-%02d-%02d %02d:%02d:%02d",
             year, mon, day, hour, minute, second);
}

bool remote_rtc_set_from_string(const char *yymmddhhmmss)
{
    if (yymmddhhmmss == NULL || strlen(yymmddhhmmss) != 12) {
        return false;
    }
    for (int i = 0; i < 12; i++) {
        if (!isdigit((unsigned char)yymmddhhmmss[i])) {
            return false;
        }
    }
    int yy = (yymmddhhmmss[0]  - '0') * 10 + (yymmddhhmmss[1]  - '0');
    int mo = (yymmddhhmmss[2]  - '0') * 10 + (yymmddhhmmss[3]  - '0');
    int dd = (yymmddhhmmss[4]  - '0') * 10 + (yymmddhhmmss[5]  - '0');
    int hh = (yymmddhhmmss[6]  - '0') * 10 + (yymmddhhmmss[7]  - '0');
    int mi = (yymmddhhmmss[8]  - '0') * 10 + (yymmddhhmmss[9]  - '0');
    int ss = (yymmddhhmmss[10] - '0') * 10 + (yymmddhhmmss[11] - '0');

    if (mo < 1 || mo > 12 || dd < 1 || dd > 31 ||
        hh > 23 || mi > 59 || ss > 59) {
        return false;
    }
    struct tm t = {0};
    t.tm_year = (2000 + yy) - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = dd;
    t.tm_hour = hh;
    t.tm_min = mi;
    t.tm_sec = ss;
    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) {
        return false;
    }
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    if (settimeofday(&tv, NULL) != 0) {
        return false;
    }
    ESP_LOGI(TAG, "RTC set to 20%02d-%02d-%02d %02d:%02d:%02d", yy, mo, dd, hh, mi, ss);
    return true;
}

static uint8_t s_to_bcd(int v)
{
    if (v < 0) v = 0;
    if (v > 99) v = 99;
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

void remote_rtc_now_bcd6(uint8_t out[6])
{
    time_t now = 0;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    int yy = (t.tm_year + 1900) - 2000;
    if (yy < 0) yy = 0;
    if (yy > 99) yy = 99;

    out[0] = s_to_bcd(yy);
    out[1] = s_to_bcd(t.tm_mon + 1);
    out[2] = s_to_bcd(t.tm_mday);
    out[3] = s_to_bcd(t.tm_hour);
    out[4] = s_to_bcd(t.tm_min);
    out[5] = s_to_bcd(t.tm_sec);
}
