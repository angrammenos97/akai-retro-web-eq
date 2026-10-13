#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"

#include "net_manager.h"
#include "secrets.h"

#define TAG "NET_MGMT"
#define WIFI_MAXIMUM_RETRY 5
#define HOSTNAME "akai-retro-web-eq"

/****************************
 * Global variable declarations *
 ****************************/

static int s_retry_num = 0;
static esp_event_handler_instance_t instance_any_id = 0;
static esp_event_handler_instance_t instance_got_ip = 0;
esp_netif_t* netif_instance = NULL;

/****************************
 * Declare helper functions *
 ****************************/

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);

static void mdns_start();

static void mdns_stop();

/****************************
 * WIFI functions *
 *****************************/

void net_manager_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_instance = esp_netif_create_default_wifi_sta();

    // Create WiFi interface in STA mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void net_manager_deinit(void)
{
    mdns_stop();
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
                                                          IP_EVENT_STA_GOT_IP,
                                                          instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
                                                          ESP_EVENT_ANY_ID,
                                                          instance_any_id));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    if (netif_instance) {
        esp_netif_destroy_default_wifi(netif_instance);
    }
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    // ESP_ERROR_CHECK(esp_netif_deinit());
    ESP_ERROR_CHECK(nvs_flash_deinit());
}

/*******************************
 * Helper function definitions *
 *******************************/

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Disconnected from AP (reason=%d); retrying (%d/%d)",
                     event->reason, s_retry_num, WIFI_MAXIMUM_RETRY);
        }
        else {
            ESP_LOGE(TAG, "Failed to connect to AP (reason=%d)", event->reason);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Assigned IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        mdns_start();
    }
}

static void mdns_start()
{
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(HOSTNAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(HOSTNAME));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));

    ESP_LOGI(TAG, "Access at http://" HOSTNAME ".local");
}

static void mdns_stop()
{
    ESP_ERROR_CHECK(mdns_service_remove("_http", "_tcp"));
    mdns_free();
}
