#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_ae_mixer.h"
#include "esp_ae_eq.h"
#include "esp_ae_rate_cvt.h"
#include "esp_ae_ch_cvt.h"
#include "esp_ae_bit_cvt.h"

#include "dsp_if.h"
#include "common.h"

#define TAG "DSP_IF"

/****************************
 * Global variable declarations *
 ****************************/

static esp_ae_mixer_handle_t    mixer_handle        = NULL;
static esp_ae_eq_handle_t       eq_handle           = NULL;
static esp_ae_rate_cvt_handle_t rate_cvt_handle     = NULL;
static esp_ae_ch_cvt_handle_t   ch_cvt_handle       = NULL;
static esp_ae_bit_cvt_handle_t  bit_cvt_handle      = NULL;

static esp_ae_sample_t          mixer_out_buffer    = NULL;
static esp_ae_sample_t          eq_out_buffer       = NULL;
static esp_ae_sample_t          rate_cvt_out_buffer = NULL;
static esp_ae_sample_t          ch_cvt_out_buffer   = NULL;
static esp_ae_sample_t          bit_cvt_out_buffer  = NULL;

/****************************
 * DSP interface functions *
 *****************************/

void dsp_init(void)
{
    // Initialize the mixer for volume control
    float volume = g_config.volume;
    esp_ae_mixer_info_t mixer[1]= {{.weight1 = volume, .weight2 = volume, .transit_time = 1}};
    esp_ae_mixer_cfg_t mixer_cfg = {
        .sample_rate        = g_config.input_sampling_rate,
        .channel            = g_config.input_channels,
        .bits_per_sample    = g_config.input_bits_per_sample,
        .src_num            = sizeof(mixer) / sizeof(mixer[0]),
        .src_info           = mixer
    };
    ESP_ERROR_CHECK(esp_ae_mixer_open(&mixer_cfg, &mixer_handle));
    ESP_ERROR_CHECK(esp_ae_mixer_set_mode(mixer_handle, 0, ESP_AE_MIXER_MODE_FADE_UPWARD));
    
    mixer_out_buffer = (esp_ae_sample_t)heap_caps_aligned_alloc(16, g_config.input_samples_num * g_config.input_channels * (g_config.input_bits_per_sample >> 3), MALLOC_CAP_8BIT);

    // Initialize the EQ effect
    esp_ae_eq_cfg_t eq_cfg = {
        .sample_rate        = g_config.input_sampling_rate,
        .channel            = g_config.input_channels,
        .bits_per_sample    = g_config.input_bits_per_sample,
        .filter_num         = g_config.eq_filters_num,
        .para               = g_config.eq_filters
    };
    ESP_ERROR_CHECK(esp_ae_eq_open(&eq_cfg, &eq_handle));

    eq_out_buffer = (esp_ae_sample_t)malloc(g_config.input_samples_num * g_config.input_channels * (g_config.input_bits_per_sample >> 3));
    if (!eq_out_buffer) {
        ESP_LOGE(TAG, "Failed to allocate EQ buffer");
        dsp_deinit();
        return;
    }

    // Initialize rate converter
    esp_ae_rate_cvt_cfg_t rt_cfg = {
        .src_rate           = g_config.input_sampling_rate,
        .dest_rate          = g_config.output_sampling_rate,
        .channel            = g_config.input_channels,
        .bits_per_sample    = g_config.input_bits_per_sample,
        .complexity         = 3,
        .perf_type          = ESP_AE_RATE_CVT_PERF_TYPE_SPEED
    };

    if (g_config.output_sampling_rate != g_config.input_sampling_rate) {
        ESP_ERROR_CHECK(esp_ae_rate_cvt_open(&rt_cfg, &rate_cvt_handle));
        ESP_ERROR_CHECK(esp_ae_rate_cvt_get_max_out_sample_num(rate_cvt_handle, g_config.input_samples_num, &g_shared_vars.output_samples_num));

        rate_cvt_out_buffer = (esp_ae_sample_t)malloc(g_shared_vars.output_samples_num * g_config.input_channels * (g_config.input_bits_per_sample >> 3));
        if (!rate_cvt_out_buffer) {
            ESP_LOGE(TAG, "Failed to allocate rate converter buffer");
            dsp_deinit();
            return;
        }
    } else {
        rate_cvt_out_buffer                 = eq_out_buffer; // If no rate conversion is needed, use the EQ buffer
        g_shared_vars.output_samples_num    = g_config.input_samples_num;
    }

    // Initialize channel converter
    esp_ae_ch_cvt_cfg_t ch_cfg = {
        .sample_rate        = g_config.output_sampling_rate,
        .bits_per_sample    = g_config.input_bits_per_sample,
        .src_ch             = g_config.input_channels,
        .dest_ch            = g_config.output_channels,
        .weight             = NULL,
        .weight_len         = 0
    };

    if (g_config.input_channels != g_config.output_channels) {
        ESP_ERROR_CHECK(esp_ae_ch_cvt_open(&ch_cfg, &ch_cvt_handle));

        ch_cvt_out_buffer = (esp_ae_sample_t)malloc(g_shared_vars.output_samples_num * g_config.output_channels * (g_config.input_bits_per_sample >> 3));
        if (!ch_cvt_out_buffer) {
            ESP_LOGE(TAG, "Failed to allocate channel converter buffer");
            dsp_deinit();
            return;
        }
    } else {
        ch_cvt_out_buffer = rate_cvt_out_buffer; // If no channel conversion is needed, use the rate converter buffer
    }

    // Initialize bit converter
    esp_ae_bit_cvt_cfg_t bit_cfg = {
        .sample_rate    = g_config.output_sampling_rate,
        .channel        = g_config.output_channels,
        .src_bits       = g_config.input_bits_per_sample,
        .dest_bits      = g_config.output_bits_per_sample
    };

    if (g_config.input_bits_per_sample != g_config.output_bits_per_sample) {
        ESP_ERROR_CHECK(esp_ae_bit_cvt_open(&bit_cfg, &bit_cvt_handle));

        bit_cvt_out_buffer = (esp_ae_sample_t)malloc(g_shared_vars.output_samples_num * g_config.output_channels * (g_config.output_bits_per_sample >> 3));
        if (!bit_cvt_out_buffer) {
            ESP_LOGE(TAG, "Failed to allocate bit converter buffer");
            dsp_deinit();
            return;
        }
    } else {
        bit_cvt_out_buffer = ch_cvt_out_buffer; // If no bit conversion is needed, use the channel converter buffer
    }
}

void dsp_deinit(void)
{
    // Deinitialize DSP handles
    if (mixer_handle) {
        esp_ae_mixer_close(mixer_handle);
    }
    if (eq_handle) {
        esp_ae_eq_close(eq_handle);
    }
    if (rate_cvt_handle) {
        esp_ae_rate_cvt_close(rate_cvt_handle);
        g_shared_vars.output_samples_num = 0; // Reset output samples number after closing rate converter
    }
    if (ch_cvt_handle) {
        esp_ae_ch_cvt_close(ch_cvt_handle);
    }
    if (bit_cvt_handle) {
        esp_ae_bit_cvt_close(bit_cvt_handle);
    }

    // Free allocated buffers
    if (bit_cvt_out_buffer && bit_cvt_out_buffer != ch_cvt_out_buffer) {
        free(bit_cvt_out_buffer);
    }
    if (ch_cvt_out_buffer && ch_cvt_out_buffer != rate_cvt_out_buffer) {
        free(ch_cvt_out_buffer);
    }
    if (rate_cvt_out_buffer && rate_cvt_out_buffer != eq_out_buffer) {
        free(rate_cvt_out_buffer);
    }
    if (eq_out_buffer) {
        free(eq_out_buffer);
    }
    if (mixer_out_buffer) {
        heap_caps_free(mixer_out_buffer);
    }
}

void dsp_process(void)
{
    // Sanity buffer checks
    if (
        g_shared_vars.dsp_input_buffer  == NULL || 
        mixer_out_buffer                == NULL ||
        eq_out_buffer                   == NULL ||
        rate_cvt_out_buffer             == NULL ||
        ch_cvt_out_buffer               == NULL ||
        bit_cvt_out_buffer              == NULL ||
        g_shared_vars.dsp_output_buffer == NULL
    )
    {
        ESP_LOGE(TAG, "Uninitialized buffer %p %p %p %p %p %p",
            g_shared_vars.dsp_input_buffer, 
            mixer_out_buffer,
            eq_out_buffer,
            rate_cvt_out_buffer,
            ch_cvt_out_buffer,
            bit_cvt_out_buffer);
        return;
    }

    // Process the audio data through the DSP pipeline
    if (mixer_handle) {
        esp_ae_sample_t in_samples[1] = {g_shared_vars.dsp_input_buffer};
        ESP_ERROR_CHECK(esp_ae_mixer_process(mixer_handle, g_config.input_samples_num, in_samples, mixer_out_buffer));
    }
    if (eq_handle) {
        ESP_ERROR_CHECK(esp_ae_eq_process(eq_handle, g_config.input_samples_num, mixer_out_buffer, eq_out_buffer));
    }
    if (rate_cvt_handle) {
        ESP_ERROR_CHECK(esp_ae_rate_cvt_process(rate_cvt_handle, eq_out_buffer, g_config.input_samples_num, rate_cvt_out_buffer, &g_shared_vars.output_samples_num));
    }
    if (ch_cvt_handle) {
        ESP_ERROR_CHECK(esp_ae_ch_cvt_process(ch_cvt_handle, g_shared_vars.output_samples_num, rate_cvt_out_buffer, ch_cvt_out_buffer));
    }
    if (bit_cvt_handle) {
        ESP_ERROR_CHECK(esp_ae_bit_cvt_process(bit_cvt_handle, g_shared_vars.output_samples_num, ch_cvt_out_buffer, bit_cvt_out_buffer));
    }

    // Copy processed sample to DSP output buffer
    memcpy(g_shared_vars.dsp_output_buffer, bit_cvt_out_buffer, g_shared_vars.output_samples_num * g_config.output_channels * (g_config.output_bits_per_sample >> 3));
}
