#include <math.h>

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
