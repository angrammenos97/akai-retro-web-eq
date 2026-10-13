#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "task_manager.h"
#include "common.h"
#include "i2s_if.h"
#include "dsp_if.h"

#define TAG             "TSK_MGMT"
#define QUEUE_LENGTH    10

/****************************
 * Global variable declarations *
 ****************************/

static QueueHandle_t input_stream        = NULL;
static QueueHandle_t output_stream       = NULL;
static volatile bool running             = false;
static TaskHandle_t receive_task_handle  = NULL;
static TaskHandle_t process_task_handle  = NULL;
static TaskHandle_t transmit_task_handle = NULL;


/****************************
 * Declare helper functions *
 ****************************/

void receive_samples_task(void* pvParameters);
void process_samples_task(void* pvParameters);
void transmit_samples_task(void* pvParameters);

/****************************
 * Task Manager functions *
 *****************************/

void task_manager_init(void)
{
    // Initialize input stream
    input_stream = xQueueCreate(QUEUE_LENGTH, sizeof(void*));
    if (input_stream == NULL) {
        ESP_LOGE(TAG, "Failed to create input_stream queue");
        task_manager_deinit();
        return;
    }

    // Initialize output stream
    output_stream = xQueueCreate(QUEUE_LENGTH, sizeof(void*));
    if (output_stream == NULL) {
        ESP_LOGE(TAG, "Failed to create output_stream queue");
        task_manager_deinit();
        return;
    }
}

void task_manager_deinit(void)
{
    if (running){
        task_manager_stop();
    }
    
    // Wait for all tasks to complete before deleting queues
    int max_wait_ms = 1000;
    int wait_interval_ms = 10;
    int elapsed = 0;
    
    while ((receive_task_handle != NULL || process_task_handle != NULL || transmit_task_handle != NULL) && 
           elapsed < max_wait_ms) {
        vTaskDelay(pdMS_TO_TICKS(wait_interval_ms));
        elapsed += wait_interval_ms;
    }
    
    // Drain input queue and free any remaining buffers
    if (input_stream) {
        void* buffer;
        while (xQueueReceive(input_stream, &buffer, 0) == pdPASS) {
            if (buffer != NULL) {
                ESP_LOGW(TAG, "Freeing orphaned buffer from input_stream queue");
                free(buffer);
            }
        }
        vQueueDelete(input_stream);
        input_stream = NULL;
    }
    
    // Drain output queue and free any remaining buffers
    if (output_stream) {
        void* buffer;
        while (xQueueReceive(output_stream, &buffer, 0) == pdPASS) {
            if (buffer != NULL) {
                ESP_LOGW(TAG, "Freeing orphaned buffer from output_stream queue");
                free(buffer);
            }
        }
        vQueueDelete(output_stream);
        output_stream = NULL;
    }
}

void task_manager_start(void)
{
    // Update running variable
    running = true;

    // Start receive task
    if (xTaskCreate(receive_samples_task, "Receive Task", 4096, NULL, 5, &receive_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create receive task");
        task_manager_deinit();
        return;
    }

    // Start process task
    if (xTaskCreate(process_samples_task, "Process Task", 4096, NULL, 5, &process_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create process task");
        task_manager_deinit();
        return;
    }

    // Start transmit task
    if (xTaskCreate(transmit_samples_task, "Transmit Task", 4096, NULL, 5, &transmit_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create transmit task");
        task_manager_deinit();
        return;
    }
}

void task_manager_stop(void)
{
    // Update running variable to signal tasks to exit
    running = false;
    
    // Give tasks time to exit cleanly (they will call vTaskDelete(NULL))
    vTaskDelay(pdMS_TO_TICKS(100));
}

/*******************************
 * Helper function definitions *
 *******************************/

/*
*/
void receive_samples_task(void* pvParameters)
{
    if (!running) {
        ESP_LOGE(TAG, "Started receive task without initialization");
    }

    while (running) {
        // Allocate new I2S Rx buffer
        void* tmp_i2s_rx_buffer = malloc(g_config.input_samples_num * g_config.input_channels * (g_config.input_bits_per_sample >> 3));
        if (tmp_i2s_rx_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate I2S Rx buffer");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Receive samples from I2S Rx channel
        g_shared_vars.i2s_rx_buffer = tmp_i2s_rx_buffer;
        i2s_receive();

        // Notify process task
        if (xQueueSend(input_stream, &tmp_i2s_rx_buffer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to notify process task");
            free(tmp_i2s_rx_buffer);
        }
    }

    receive_task_handle = NULL;
    vTaskDelete(NULL);
}

void process_samples_task(void* pvParameters)
{
    if (!running) {
        ESP_LOGE(TAG, "Started process task without initialization");
    }

    while (running) {
        // Receive notification from receive task
        void* tmp_dsp_input_buffer;
        if (xQueueReceive(input_stream, &tmp_dsp_input_buffer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to receive notification from receive task");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (tmp_dsp_input_buffer == NULL) {
            ESP_LOGE(TAG, "Process task received NULL DSP input pointer");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Allocate DSP output buffer
        void* tmp_dsp_output_buffer = malloc(g_shared_vars.output_samples_num * g_config.output_channels * (g_config.output_bits_per_sample >> 3));
        if (tmp_dsp_output_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DSP output buffer");
            vTaskDelay(pdMS_TO_TICKS(100));
            free(tmp_dsp_input_buffer);
            continue;
        }

        // Process input samples
        g_shared_vars.dsp_input_buffer = tmp_dsp_input_buffer;
        g_shared_vars.dsp_output_buffer = tmp_dsp_output_buffer;
        dsp_process();

        // Free DSP input buffer
        free(tmp_dsp_input_buffer);

        // Notify transmit task
        if (xQueueSend(output_stream, &tmp_dsp_output_buffer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to notify transmit task");
            free(tmp_dsp_output_buffer);
        }
    }
    process_task_handle = NULL;
    vTaskDelete(NULL);
}

void transmit_samples_task(void* pvParameters)
{
    if (!running){
        ESP_LOGE(TAG, "Started transmit task without initialization");
    }

    while (running){
        // Receive notification from process task
        void* tmp_i2s_tx_buffer;
        if (xQueueReceive(output_stream, &tmp_i2s_tx_buffer, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Failed to receive notification from process task");
            continue;
        }
        if (tmp_i2s_tx_buffer == NULL) {
            ESP_LOGE(TAG, "Transmit task received NULL Tx pointer");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Transmit samples to I2S Tx channel
        g_shared_vars.i2s_tx_buffer = tmp_i2s_tx_buffer;
        i2s_transmit();

        // Free I2S Tx buffer
        free(tmp_i2s_tx_buffer);
    }
    transmit_task_handle = NULL;
    vTaskDelete(NULL);
}
