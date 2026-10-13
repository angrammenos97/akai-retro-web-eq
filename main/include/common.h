#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include "driver/i2s_std.h"
#include "esp_ae_eq.h"

// Define the configuration structure
typedef struct configuration_t {
    // Input I2S channel configuration
    i2s_std_gpio_config_t       rx_gpio_config;         // GPIO pin configuration for the I2S Rx channel
    uint32_t                    input_sampling_rate;    // Sampling rate in Hz (e.g., 44100, 48000, 96000)
    uint32_t                    input_bits_per_sample;  // Number of bits per sample (e.g., 16, 24, 32)
    uint32_t                    input_channels;         // Number of audio channels (e.g., 1 for mono, 2 for stereo)
    // Output I2S channel configuration
    i2s_std_gpio_config_t       tx_gpio_config;         // GPIO pin configuration for the I2S Tx channel
    uint32_t                    output_sampling_rate;   // Sampling rate in Hz (e.g., 44100, 48000, 96000)
    uint32_t                    output_bits_per_sample; // Number of bits per sample (e.g., 16, 24, 32)
    uint32_t                    output_channels;        // Number of audio channels (e.g., 1 for mono, 2 for stereo)
    // Audio DSP configuration
    uint32_t                    input_samples_num;      // Number of samples to process in each DSP operation
    esp_ae_eq_filter_para_t*    eq_filters;             // Pointer to an array of equalizer filter parameters
    uint8_t                     eq_filters_num;         // Number of equalizer filters in the array
    float                       volume;                 // Volume level (0.0 to 1.0) for the audio output
} configuration_t;

extern configuration_t g_config; // Global configuration variable

// Define shared variables
typedef struct shared_vars_t {
    // I2S runtime variables
    void*       i2s_rx_buffer;      // Pointer to the audio data buffer for I2S Rx channel
    void*       i2s_tx_buffer;      // Pointer to the audio data buffer for I2S Tx channel
    // Audio DSP runtime variables
    uint32_t    output_samples_num; // Number of samples to process in each DSP operation (after rate conversion)
    void*       dsp_input_buffer;   // Pointer to the input buffer for DSP processing
    void*       dsp_output_buffer;  // Pointer to the output buffer for DSP processing

} shared_vars_t;

extern shared_vars_t g_shared_vars; // Global shared variables

/*
 * @brief Initialize the common variables and configurations to default values
 * @note  This function should be called at the beginning of the app_main function to set up the default configuration and initialize shared variables.
 */
void common_init(void);

void app_start(void);

void app_stop(void);

void app_restart(void);

#endif // COMMON_H