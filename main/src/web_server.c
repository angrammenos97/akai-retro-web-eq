#include <math.h>
#include <stdbool.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"

#include "web_server.h"
#include "common.h"
#include "memory.h"

#define TAG "WEB_SERVER"

/****************************
 * Global variable declarations *
 ****************************/

httpd_handle_t server = NULL;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t vinyl_png_start[] asm("_binary_vinyl_png_start");
extern const uint8_t vinyl_png_end[]   asm("_binary_vinyl_png_end");

/****************************
 * Declare helper functions *
 ****************************/

static esp_err_t get_page_handler(httpd_req_t *req);
static esp_err_t get_favicon_handler(httpd_req_t *req);
static esp_err_t get_settings_handler(httpd_req_t *req);
static esp_err_t set_settings_handler(httpd_req_t *req);
static esp_err_t save_settings_handler(httpd_req_t *req);
static esp_err_t get_audio_config_handler(httpd_req_t *req);
static esp_err_t set_audio_config_handler(httpd_req_t *req);
static esp_err_t save_audio_config_handler(httpd_req_t *req);
static bool json_integer_in_range(cJSON *root, const char *name, int minimum, int maximum, int *value);
static esp_err_t update_audio_config(httpd_req_t *req, bool save);

/****************************
 * Web Server functions *
 *****************************/

void web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    // Start server
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // Register get page
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_page_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_get));

    httpd_uri_t uri_favicon = {
        .uri = "/favicon.png",
        .method = HTTP_GET,
        .handler = get_favicon_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_favicon));

    httpd_uri_t uri_get_settings = {
        .uri = "/api/equalizer",
        .method = HTTP_GET,
        .handler = get_settings_handler,
    };
    httpd_register_uri_handler(server, &uri_get_settings);
       
    httpd_uri_t uri_post = {
        .uri = "/api/equalizer",
        .method = HTTP_POST,
        .handler = set_settings_handler,
    };
    httpd_register_uri_handler(server, &uri_post);
    
    httpd_uri_t uri_save = {
        .uri = "/api/equalizer/save",
        .method = HTTP_POST,
        .handler = save_settings_handler,
    };
    httpd_register_uri_handler(server, &uri_save);

    httpd_uri_t uri_get_audio_config = {
        .uri = "/api/audio",
        .method = HTTP_GET,
        .handler = get_audio_config_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_get_audio_config));

    httpd_uri_t uri_set_audio_config = {
        .uri = "/api/audio",
        .method = HTTP_POST,
        .handler = set_audio_config_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_set_audio_config));

    httpd_uri_t uri_save_audio_config = {
        .uri = "/api/audio/save",
        .method = HTTP_POST,
        .handler = save_audio_config_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_save_audio_config));
    
}

void web_server_stop(void)
{
    if (server != NULL) {
        ESP_ERROR_CHECK(httpd_stop(server));
    }
}

/*******************************
 * Helper function definitions *
 *******************************/

static esp_err_t get_page_handler(httpd_req_t *req)
{
    // Calculate the actual size of the embedded HTML file
    const size_t index_html_size = (index_html_end - index_html_start);

    // Set the content type and send the HTML file as the response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);

    return ESP_OK;
}

static esp_err_t get_favicon_handler(httpd_req_t *req)
{
    const size_t favicon_size = vinyl_png_end - vinyl_png_start;

    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, (const char *)vinyl_png_start, favicon_size);

    return ESP_OK;
}

static esp_err_t get_settings_handler(httpd_req_t *req)
{
    // Create JSON response with current settings
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "band1", roundf(g_config.eq_filters[0].gain));
    cJSON_AddNumberToObject(root, "band2", roundf(g_config.eq_filters[1].gain));
    cJSON_AddNumberToObject(root, "band3", roundf(g_config.eq_filters[2].gain));
    cJSON_AddNumberToObject(root, "band4", roundf(g_config.eq_filters[3].gain));
    cJSON_AddNumberToObject(root, "band5", roundf(g_config.eq_filters[4].gain));
    cJSON_AddNumberToObject(root, "band6", roundf(g_config.eq_filters[5].gain));
    cJSON_AddNumberToObject(root, "volume", roundf(g_config.volume * 100.0f));
    
    char *response_str = cJSON_Print(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));
    
    free(response_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

static esp_err_t get_audio_config_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "rx_mclk", g_config.rx_gpio_config.mclk);
    cJSON_AddNumberToObject(root, "rx_bclk", g_config.rx_gpio_config.bclk);
    cJSON_AddNumberToObject(root, "rx_ws", g_config.rx_gpio_config.ws);
    cJSON_AddNumberToObject(root, "rx_din", g_config.rx_gpio_config.din);
    cJSON_AddNumberToObject(root, "input_rate", g_config.input_sampling_rate);
    cJSON_AddNumberToObject(root, "input_bits", g_config.input_bits_per_sample);
    cJSON_AddNumberToObject(root, "input_channels", g_config.input_channels);
    cJSON_AddNumberToObject(root, "tx_mclk", g_config.tx_gpio_config.mclk);
    cJSON_AddNumberToObject(root, "tx_bclk", g_config.tx_gpio_config.bclk);
    cJSON_AddNumberToObject(root, "tx_ws", g_config.tx_gpio_config.ws);
    cJSON_AddNumberToObject(root, "tx_dout", g_config.tx_gpio_config.dout);
    cJSON_AddNumberToObject(root, "output_rate", g_config.output_sampling_rate);
    cJSON_AddNumberToObject(root, "output_bits", g_config.output_bits_per_sample);
    cJSON_AddNumberToObject(root, "output_channels", g_config.output_channels);

    char *response_str = cJSON_PrintUnformatted(root);
    if (response_str == NULL) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unable to create response");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t result = httpd_resp_send(req, response_str, HTTPD_RESP_USE_STRLEN);
    free(response_str);
    cJSON_Delete(root);
    return result;
}

static esp_err_t set_audio_config_handler(httpd_req_t *req)
{
    return update_audio_config(req, false);
}

static esp_err_t save_audio_config_handler(httpd_req_t *req)
{
    return update_audio_config(req, true);
}

static bool json_integer_in_range(cJSON *root, const char *name, int minimum, int maximum, int *value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < minimum || item->valuedouble > maximum ||
        floor(item->valuedouble) != item->valuedouble) {
        return false;
    }
    *value = item->valueint;
    return true;
}

static esp_err_t update_audio_config(httpd_req_t *req, bool save)
{
    char buffer[512];
    if (req->content_len == 0 || req->content_len >= sizeof(buffer)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size");
        return ESP_FAIL;
    }

    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buffer + received, req->content_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete request");
            return ESP_FAIL;
        }
        received += ret;
    }
    buffer[received] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    int values[14];
    const char *keys[] = {
        "rx_mclk", "rx_bclk", "rx_ws", "rx_din", "input_rate", "input_bits", "input_channels",
        "tx_mclk", "tx_bclk", "tx_ws", "tx_dout", "output_rate", "output_bits", "output_channels"
    };
    bool valid = true;
    for (size_t i = 0; i < 14 && valid; ++i) {
        int minimum = (i == 3 || i == 10) ? -1 : 0;
        int maximum = (i == 3 || i == 10) ? 48 : 48;
        if (i == 4 || i == 11) {
            minimum = 8000;
            maximum = 192000;
        } else if (i == 5 || i == 12) {
            minimum = 16;
            maximum = 32;
        } else if (i == 6 || i == 13) {
            minimum = 1;
            maximum = 2;
        }
        valid = json_integer_in_range(root, keys[i], minimum, maximum, &values[i]);
    }
    if (valid && ((values[4] != 44100 && values[4] != 48000 && values[4] != 88200 &&
                   values[4] != 96000 && values[4] != 176400 && values[4] != 192000) ||
                  (values[11] != 44100 && values[11] != 48000 && values[11] != 88200 &&
                   values[11] != 96000 && values[11] != 176400 && values[11] != 192000) ||
                  (values[5] != 16 && values[5] != 24 && values[5] != 32) ||
                  (values[12] != 16 && values[12] != 24 && values[12] != 32))) {
        valid = false;
    }
    if (!valid) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid audio configuration");
        return ESP_FAIL;
    }

    g_config.rx_gpio_config.mclk = values[0];
    g_config.rx_gpio_config.bclk = values[1];
    g_config.rx_gpio_config.ws = values[2];
    g_config.rx_gpio_config.din = values[3];
    g_config.input_sampling_rate = values[4];
    g_config.input_bits_per_sample = values[5];
    g_config.input_channels = values[6];
    g_config.tx_gpio_config.mclk = values[7];
    g_config.tx_gpio_config.bclk = values[8];
    g_config.tx_gpio_config.ws = values[9];
    g_config.tx_gpio_config.dout = values[10];
    g_config.output_sampling_rate = values[11];
    g_config.output_bits_per_sample = values[12];
    g_config.output_channels = values[13];
    cJSON_Delete(root);

    if (save) {
        save_config();
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"success\"}");
    app_restart();
    return ESP_OK;
}

static esp_err_t set_settings_handler(httpd_req_t *req)
{
    char buffer[256];
    int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
        return ESP_FAIL;
    }
    
    buffer[ret] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract band values
    cJSON *band1 = cJSON_GetObjectItem(root, "band1");
    cJSON *band2 = cJSON_GetObjectItem(root, "band2");
    cJSON *band3 = cJSON_GetObjectItem(root, "band3");
    cJSON *band4 = cJSON_GetObjectItem(root, "band4");
    cJSON *band5 = cJSON_GetObjectItem(root, "band5");
    cJSON *band6 = cJSON_GetObjectItem(root, "band6");
    cJSON *volume = cJSON_GetObjectItem(root, "volume");
    
    if (band1 && band2 && band3 && band4 && band5 && band6) {
        g_config.eq_filters[0].gain = band1->valuedouble;
        g_config.eq_filters[1].gain = band2->valuedouble;
        g_config.eq_filters[2].gain = band3->valuedouble;
        g_config.eq_filters[3].gain = band4->valuedouble;
        g_config.eq_filters[4].gain = band5->valuedouble;
        g_config.eq_filters[5].gain = band6->valuedouble;
        if (volume) {
            g_config.volume = volume->valuedouble / 100.0f;
        }
        
        ESP_LOGI(TAG, "Equalizer settings updated: 60Hz=%.1fdB, 250Hz=%.1fdB, 1kHz=%.1fdB, 4kHz=%.1fdB, 8kHz=%.1fdB, 16kHz=%.1fdB, Volume=%.1f%%",
                    g_config.eq_filters[0].gain,
                    g_config.eq_filters[1].gain,
                    g_config.eq_filters[2].gain,
                    g_config.eq_filters[3].gain,
                    g_config.eq_filters[4].gain,
                    g_config.eq_filters[5].gain,
                    g_config.volume * 100.0f);
        
        // Send success response
        httpd_resp_set_type(req, "application/json");
        const char *response = "{\"status\":\"success\",\"message\":\"Settings applied\"}";
        httpd_resp_send(req, response, strlen(response));
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing band values");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_Delete(root);

    // Restart app
    app_restart();

    return ESP_OK;
}

static esp_err_t save_settings_handler(httpd_req_t *req)
{
    char buffer[256];
    int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
        return ESP_FAIL;
    }
    
    buffer[ret] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Extract band values
    cJSON *band1 = cJSON_GetObjectItem(root, "band1");
    cJSON *band2 = cJSON_GetObjectItem(root, "band2");
    cJSON *band3 = cJSON_GetObjectItem(root, "band3");
    cJSON *band4 = cJSON_GetObjectItem(root, "band4");
    cJSON *band5 = cJSON_GetObjectItem(root, "band5");
    cJSON *band6 = cJSON_GetObjectItem(root, "band6");
    cJSON *volume = cJSON_GetObjectItem(root, "volume");
    
    if (band1 && band2 && band3 && band4 && band5 && band6) {
        g_config.eq_filters[0].gain = band1->valuedouble;
        g_config.eq_filters[1].gain = band2->valuedouble;
        g_config.eq_filters[2].gain = band3->valuedouble;
        g_config.eq_filters[3].gain = band4->valuedouble;
        g_config.eq_filters[4].gain = band5->valuedouble;
        g_config.eq_filters[5].gain = band6->valuedouble;
        if (volume) {
            g_config.volume = volume->valuedouble / 100.0f;
        }

        // Save into memory
        save_config();
        
        // Send success response
        httpd_resp_set_type(req, "application/json");
        const char *response = "{\"status\":\"success\",\"message\":\"Settings applied\"}";
        httpd_resp_send(req, response, strlen(response));
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing band values");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON_Delete(root);

    // Restart app
    app_restart();

    return ESP_OK;
}
