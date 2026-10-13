#include "nvs_flash.h"
#include "esp_log.h"

#include "memory.h"
#include "common.h"

#define TAG "MEM"
#define NVS_NAMESPACE "g_config"

/****************************
 * Memory functions *
 *****************************/

void save_config(void)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band1", (int16_t)(g_config.eq_filters[0].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band2", (int16_t)(g_config.eq_filters[1].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band3", (int16_t)(g_config.eq_filters[2].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band4", (int16_t)(g_config.eq_filters[3].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band5", (int16_t)(g_config.eq_filters[4].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "band6", (int16_t)(g_config.eq_filters[5].gain * 1000.0f)));
    ESP_ERROR_CHECK(nvs_set_i16(nvs_handle, "volume", (int16_t)(g_config.volume* 1000.0f)));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));

    ESP_LOGI(TAG, "Configuration saved: 60Hz=%.1fdB, 250Hz=%.1fdB, 1kHz=%.1fdB, 4kHz=%.1fdB, 8kHz=%.1fdB, 16kHz=%.1fdB, Volume=%.1f%%",
                    g_config.eq_filters[0].gain,
                    g_config.eq_filters[1].gain,
                    g_config.eq_filters[2].gain,
                    g_config.eq_filters[3].gain,
                    g_config.eq_filters[4].gain,
                    g_config.eq_filters[5].gain,
                    g_config.volume * 100.0f);

    nvs_close(nvs_handle);
}

void load_config(void)
{
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle));

    int16_t value;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band1", &value));
    g_config.eq_filters[0].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band2", &value));
    g_config.eq_filters[1].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band3", &value));
    g_config.eq_filters[2].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band4", &value));
    g_config.eq_filters[3].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band5", &value));
    g_config.eq_filters[4].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "band6", &value));
    g_config.eq_filters[5].gain = (float)value / 1000.0f;
    ESP_ERROR_CHECK(nvs_get_i16(nvs_handle, "volume", &value));
    g_config.volume = (float)value / 1000.0f;

    ESP_LOGI(TAG, "Configuration loaded: 60Hz=%.1fdB, 250Hz=%.1fdB, 1kHz=%.1fdB, 4kHz=%.1fdB, 8kHz=%.1fdB, 16kHz=%.1fdB, Volume=%.1f%%",
                    g_config.eq_filters[0].gain,
                    g_config.eq_filters[1].gain,
                    g_config.eq_filters[2].gain,
                    g_config.eq_filters[3].gain,
                    g_config.eq_filters[4].gain,
                    g_config.eq_filters[5].gain,
                    g_config.volume * 100.0f);

    nvs_close(nvs_handle);
}
