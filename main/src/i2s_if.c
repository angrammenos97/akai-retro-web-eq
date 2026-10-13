#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "i2s_if.h"
#include "common.h"

#define TAG "I2S_IF"

/****************************
 * Global variable declarations *
 ****************************/

static i2s_chan_handle_t rx_chan;
static i2s_chan_handle_t tx_chan;

/****************************
 * Declare helper functions *
 ****************************/

i2s_mclk_multiple_t get_mclk_multiple(uint32_t bits_per_sample, uint32_t sample_rate);

/**************************
* I2S interface functions *
***************************/

void i2s_init(void)
{
    // Configure I2S Rx channel
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_chan));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(g_config.input_sampling_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            g_config.input_bits_per_sample, 
            g_config.input_channels
        ),
        .gpio_cfg = g_config.rx_gpio_config
    };
    rx_std_cfg.clk_cfg.mclk_multiple = get_mclk_multiple(g_config.input_bits_per_sample, g_config.input_sampling_rate);
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));

    // Configure I2S Tx channel
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(1, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(g_config.output_sampling_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            g_config.output_bits_per_sample, 
            g_config.output_channels
        ),
        .gpio_cfg = g_config.tx_gpio_config
    };
    tx_std_cfg.clk_cfg.mclk_multiple = get_mclk_multiple(g_config.output_bits_per_sample, g_config.output_sampling_rate);
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));

    // Start the channels
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}

void i2s_deinit(void)
{
    // Stop the channels
    i2s_chan_info_t channel_info;
    ESP_ERROR_CHECK(i2s_channel_get_info(tx_chan, &channel_info));
    if (channel_info.is_enabled) {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_chan));
    }
    ESP_ERROR_CHECK(i2s_channel_get_info(rx_chan, &channel_info));
    if (channel_info.is_enabled) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_chan));
    }

    // Delete the channels
    if (rx_chan) {
        ESP_ERROR_CHECK(i2s_del_channel(rx_chan));
    }
    if (tx_chan) {
        ESP_ERROR_CHECK(i2s_del_channel(tx_chan));
    }
}

void i2s_receive()
{
    // Check if the Rx channel is initialized
    if (g_shared_vars.i2s_rx_buffer == NULL || rx_chan == NULL) {
        ESP_LOGE(TAG, "Rx channel is not initialized");
        i2s_deinit();
        return;
    }
    i2s_chan_info_t channel_info;
    ESP_ERROR_CHECK(i2s_channel_get_info(rx_chan, &channel_info));
    if (!channel_info.is_enabled) {
        ESP_LOGE(TAG, "Rx channel is not enabled");
        i2s_deinit();
        return;
    }

    // Read audio data from the Rx channel
    size_t bytes_read = 0;
    size_t bytes_to_read = g_config.input_samples_num * g_config.input_channels * (g_config.input_bits_per_sample >> 3);
    while (bytes_read < bytes_to_read) {
        size_t bytes_received = 0;
        ESP_ERROR_CHECK(
            i2s_channel_read(
                rx_chan, 
                (char*)g_shared_vars.i2s_rx_buffer + bytes_read, bytes_to_read - bytes_read, 
                &bytes_received, 
                portMAX_DELAY
            )
        );
        bytes_read += bytes_received;
    }
}

void i2s_transmit()
{
    // Check if the Tx channel is initialized
    if (g_shared_vars.i2s_tx_buffer == NULL || tx_chan == NULL || g_shared_vars.output_samples_num == 0) {
        ESP_LOGE(TAG, "Tx channel is not initialized");
        i2s_deinit();
        return;
    }
    i2s_chan_info_t channel_info;
    ESP_ERROR_CHECK(i2s_channel_get_info(tx_chan, &channel_info));
    if (!channel_info.is_enabled) {
        ESP_LOGE(TAG, "Tx channel is not enabled");
        i2s_deinit();
        return;
    }

    // Write audio data to the Tx channel
    size_t bytes_written = 0;
    size_t bytes_to_write = g_shared_vars.output_samples_num * g_config.output_channels * (g_config.output_bits_per_sample >> 3);
    while (bytes_written < bytes_to_write) {
        size_t bytes_sent = 0;
        ESP_ERROR_CHECK(
            i2s_channel_write(
                tx_chan, 
                (char*)g_shared_vars.i2s_tx_buffer + bytes_written, bytes_to_write - bytes_written, 
                &bytes_sent, 
                portMAX_DELAY
            )
        );
        bytes_written += bytes_sent;
    }
}

/*******************************
 * Helper function definitions *
 *******************************/

/*
 * @brief Get the MCLK multiple based on bits per sample and sample rate
 *
 * @param bits_per_sample: The number of bits per sample (e.g., 16, 24, 32)
 * @param sample_rate: The sample rate in Hz (e.g., 44100, 48000, 96000)
 * @return The MCLK multiple to be used in the I2S clock configuration
 */
i2s_mclk_multiple_t get_mclk_multiple(uint32_t bits_per_sample, uint32_t sample_rate)
{
    switch (bits_per_sample) {
        case 32:    // 32-bit
            switch (sample_rate) {
                case 96000:
                    return 64;
                case 192000:
                    return 32;
                default:
                    return I2S_MCLK_MULTIPLE_128;
            }
        case 24:    // 24-bit
            switch (sample_rate) {
                case 96000:
                    return I2S_MCLK_MULTIPLE_192;
                case 192000:
                    return 96;
                default:
                    return I2S_MCLK_MULTIPLE_384;
            }
        case 16:    // 16-bit
        default:
            switch (sample_rate) {
                case 96000:
                    return I2S_MCLK_MULTIPLE_128;
                case 192000:
                    return 64;
                default:
                    return I2S_MCLK_MULTIPLE_256;
            }
    }
}
