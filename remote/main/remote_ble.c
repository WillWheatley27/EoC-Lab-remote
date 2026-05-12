#include "remote_ble.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "remote_rtc.h"

static const char *TAG = "ble";
static SemaphoreHandle_t s_sync_sem;
static uint8_t s_own_addr_type;

static void s_on_sync(void)
{
    ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (s_sync_sem) {
        xSemaphoreGive(s_sync_sem);
    }
}

static void s_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t remote_ble_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE init failed (%s)", esp_err_to_name(err));
        return err;
    }

    s_sync_sem = xSemaphoreCreateBinary();
    if (s_sync_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ble_hs_cfg.sync_cb = s_on_sync;
    nimble_port_freertos_init(s_host_task);

    // Wait up to 5 s for the controller to come up.
    if (xSemaphoreTake(s_sync_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "BLE host did not sync in 5s");
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "BLE host ready (own_addr_type=%d)", s_own_addr_type);
    return ESP_OK;
}

// Build the 16-byte big-endian UUID and pack it into NimBLE's little-endian
// ble_uuid128_t storage. Same byte order the main board parser expects.
static void s_build_trigger_uuid(uint8_t tt_byte, ble_uuid128_t *out)
{
    uint8_t be[16] = {0};
    be[8] = tt_byte;
    be[9] = 0x00;
    uint8_t bcd[6];
    remote_rtc_now_bcd6(bcd);
    memcpy(&be[10], bcd, 6);

    out->u.type = BLE_UUID_TYPE_128;
    for (int i = 0; i < 16; ++i) {
        out->value[i] = be[15 - i];   // value is little-endian
    }
}

esp_err_t remote_ble_advertise_trigger(uint8_t tt_byte, uint32_t window_ms)
{
    ble_uuid128_t uuid;
    s_build_trigger_uuid(tt_byte, &uuid);

    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_set_fields rc=%d", rc);
        return ESP_FAIL;
    }

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;     // non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x0030;                     // ~30 ms
    adv_params.itvl_max = 0x0050;                     // ~50 ms

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_start rc=%d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Advertising TT=0x%02X for %u ms", tt_byte, (unsigned)window_ms);
    vTaskDelay(pdMS_TO_TICKS(window_ms));
    ble_gap_adv_stop();
    return ESP_OK;
}
