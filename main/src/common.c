#include "esp_log.h"

#include "common.h"

#include "dsp_if.h"
#include "i2s_if.h"
#include "task_manager.h"
#include "net_manager.h"
#include "web_server.h"
#include "memory.h"

#define TAG "APP"

/****************************
 * Global variable declarations *
 ****************************/
configuration_t g_config;
shared_vars_t   g_shared_vars;

esp_ae_eq_filter_para_t default_eq_filters[] = {
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 60,    .q = 0.7f, .gain = 0.0f},
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 150,   .q = 0.7f, .gain = 0.0f},
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 400,   .q = 0.7f, .gain = 0.0f},
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 1000,  .q = 0.7f, .gain = 0.0f},
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 2400,  .q = 0.7f, .gain = 0.0f},
    {.filter_type = ESP_AE_EQ_FILTER_PEAK, .fc = 10000, .q = 0.7f, .gain = 0.0f}
};

/****************************
 * Common functions *
 *****************************/

void common_init(void)
{
    g_config = (configuration_t){
        // Input I2S channel configuration
        .rx_gpio_config = {
            .mclk               = GPIO_NUM_18,
            .bclk               = GPIO_NUM_17,
            .ws                 = GPIO_NUM_15,
            .dout               = -1,
            .din                = GPIO_NUM_16,
            .invert_flags       = {0}
        },
        .input_sampling_rate    = 96000, //48000,
        .input_bits_per_sample  = 24,
        .input_channels         = 2,
        // Output I2S channel configuration
        .tx_gpio_config = {
            .mclk               = GPIO_NUM_4,
            .bclk               = GPIO_NUM_6,
            .ws                 = GPIO_NUM_5,
            .dout               = GPIO_NUM_7,
            .din                = -1,
            .invert_flags       = {0}
        },
        .output_sampling_rate   = 96000, //48000,
        .output_bits_per_sample = 24,
        .output_channels        = 2,
        // Audio DSP configuration
        .input_samples_num      = 4096,
        .eq_filters             = default_eq_filters,
        .eq_filters_num         = sizeof(default_eq_filters) / sizeof(default_eq_filters[0]),
        .volume                 = 0.40f
    };

    // Initialize shared variables to zero
    g_shared_vars = (shared_vars_t){0};

    ESP_LOGI(TAG, "Common variables set to default");
}

void app_start(void)
{
    // Single init modules
    net_manager_init();
    web_server_start();

    // Runtime init modules
    load_config();
    dsp_init();
    i2s_init();
    task_manager_init();
    task_manager_start();

    ESP_LOGI(TAG, "Application started");
}

void app_stop(void)
{
    // Runtime deinit modules
    task_manager_stop();
    task_manager_deinit();
    dsp_deinit();
    i2s_deinit();

    // Single deinit modules
    web_server_stop();
    net_manager_deinit();

    ESP_LOGI(TAG, "Application stoped");
}

void app_restart(void)
{
    // Runtime deinit modules
    task_manager_stop();
    task_manager_deinit();
    dsp_deinit();
    i2s_deinit();

    // Runtime init modules
    dsp_init();
    i2s_init();
    task_manager_init();
    task_manager_start();

    ESP_LOGI(TAG, "Application restated");
}
