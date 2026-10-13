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
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "rx_mclk", g_config.rx_gpio_config.mclk));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "rx_bclk", g_config.rx_gpio_config.bclk));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "rx_ws", g_config.rx_gpio_config.ws));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "rx_din", g_config.rx_gpio_config.din));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "input_rate", g_config.input_sampling_rate));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "input_bits", g_config.input_bits_per_sample));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "input_channels", g_config.input_channels));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "tx_mclk", g_config.tx_gpio_config.mclk));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "tx_bclk", g_config.tx_gpio_config.bclk));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "tx_ws", g_config.tx_gpio_config.ws));
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, "tx_dout", g_config.tx_gpio_config.dout));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "output_rate", g_config.output_sampling_rate));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "output_bits", g_config.output_bits_per_sample));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "output_channels", g_config.output_channels));

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

    int32_t pin;
    uint32_t audio_value;
#define LOAD_PIN(key, field) do { if (nvs_get_i32(nvs_handle, key, &pin) == ESP_OK) g_config.field = pin; } while (0)
#define LOAD_AUDIO_VALUE(key, field) do { if (nvs_get_u32(nvs_handle, key, &audio_value) == ESP_OK) g_config.field = audio_value; } while (0)
    LOAD_PIN("rx_mclk", rx_gpio_config.mclk);
    LOAD_PIN("rx_bclk", rx_gpio_config.bclk);
    LOAD_PIN("rx_ws", rx_gpio_config.ws);
    LOAD_PIN("rx_din", rx_gpio_config.din);
    LOAD_AUDIO_VALUE("input_rate", input_sampling_rate);
    LOAD_AUDIO_VALUE("input_bits", input_bits_per_sample);
    LOAD_AUDIO_VALUE("input_channels", input_channels);
    LOAD_PIN("tx_mclk", tx_gpio_config.mclk);
    LOAD_PIN("tx_bclk", tx_gpio_config.bclk);
    LOAD_PIN("tx_ws", tx_gpio_config.ws);
    LOAD_PIN("tx_dout", tx_gpio_config.dout);
    LOAD_AUDIO_VALUE("output_rate", output_sampling_rate);
    LOAD_AUDIO_VALUE("output_bits", output_bits_per_sample);
    LOAD_AUDIO_VALUE("output_channels", output_channels);
#undef LOAD_PIN
#undef LOAD_AUDIO_VALUE

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
