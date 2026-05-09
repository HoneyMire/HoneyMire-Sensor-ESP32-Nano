#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "telnet_server.h"
#include "display_task.h"

static const char *TAG = "honeypot_main";
static bool sntp_started = false;

// WiFi configuration - adjust as needed
#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASSWORD

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying WiFi connection...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        if (!sntp_started) {
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_init();
            sntp_started = true;
            ESP_LOGI(TAG, "Started SNTP time sync");
        }
    }
}

static void wifi_init_sta(void) {
    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished.");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting HiLinux NVR Box Honeypot for ESP32-C3");
    
    // Start display task early
    display_task_start();
    display_show_boot_logo();
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Print chip info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: %s, Features: %d cores, %s%s%s%s",
            CONFIG_IDF_TARGET,
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
            (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
            (chip_info.features & CHIP_FEATURE_IEEE802154) ? "IEEE802154 " : "");
    
    // Initialize network
    if (WIFI_SSID[0] != '\0') {
    wifi_init_sta();
    
    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
    // For Ethernet or if WiFi is not configured
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // If using Ethernet on ESP32-C3
    #ifdef CONFIG_ETHERNET_ENABLED
    // Ethernet initialization would go here
    ESP_LOGI(TAG, "Ethernet initialization (configure as needed)");
    #endif
    
    ESP_LOGI(TAG, "Waiting for network...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    
    // Start telnet server
    start_telnet_server();
}
