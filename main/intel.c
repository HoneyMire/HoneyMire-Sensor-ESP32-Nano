#include <stdio.h>
#include <string.h>
#include "intel.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "intel";

bool intel_ip_is_private(const char *ip) {
    if (!ip || ip[0] == '\0') return true;
    if (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "::1") == 0) return true;
    if (strncmp(ip, "10.", 3) == 0) return true;
    if (strncmp(ip, "192.168.", 8) == 0) return true;
    if (strncmp(ip, "169.254.", 8) == 0) return true;
    if (strncmp(ip, "172.", 4) == 0) {
        int second;
        if (sscanf(ip + 4, "%d", &second) == 1) {
            if (second >= 16 && second <= 31) return true;
        }
    }
    return false;
}

static char cached_pulse_id[64] = {0};

static void load_pulse_id() {
    nvs_handle_t handle;
    if (nvs_open("otx_cache", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(cached_pulse_id);
        nvs_get_str(handle, "pulse_id", cached_pulse_id, &size);
        nvs_close(handle);
    }
}

static void save_pulse_id(const char *id) {
    nvs_handle_t handle;
    if (nvs_open("otx_cache", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "pulse_id", id);
        nvs_commit(handle);
        nvs_close(handle);
    }
    strncpy(cached_pulse_id, id, sizeof(cached_pulse_id) - 1);
}

static char* create_pulse(const attack_info_t *seed) {
    esp_http_client_config_t config = {
        .url = "https://otx.alienvault.com/api/v1/pulses/create",
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", CONFIG_OTX_PULSE_NAME);
    cJSON_AddStringToObject(root, "description", "Live capture of brute-force login attempts against an ESP32-C3 honeypot (HoneyMistNano).");
    cJSON_AddBoolToObject(root, "public", false);
    
    cJSON *tags = cJSON_AddArrayToObject(root, "tags");
    cJSON_AddItemToArray(tags, cJSON_CreateString("honeypot"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("brute-force"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("telnet"));

    cJSON *inds = cJSON_AddArrayToObject(root, "indicators");
    cJSON *i0 = cJSON_CreateObject();
    cJSON_AddStringToObject(i0, "type", "IPv4");
    cJSON_AddStringToObject(i0, "indicator", seed->ip);
    cJSON_AddStringToObject(i0, "role", "bruteforce");
    char title[64];
    snprintf(title, sizeof(title), "%s login attempt", seed->protocol);
    cJSON_AddStringToObject(i0, "title", title);
    cJSON_AddItemToArray(inds, i0);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    esp_http_client_set_header(client, "X-OTX-API-KEY", CONFIG_OTX_API_KEY);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    char *new_id = NULL;

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            char buffer[1024];
            int len = esp_http_client_read_response(client, buffer, sizeof(buffer) - 1);
            if (len > 0) {
                buffer[len] = '\0';
                cJSON *resp = cJSON_Parse(buffer);
                if (resp) {
                    cJSON *id_node = cJSON_GetObjectItem(resp, "id");
                    if (cJSON_IsString(id_node)) {
                        new_id = strdup(id_node->valuestring);
                    }
                    cJSON_Delete(resp);
                }
            }
        } else {
            ESP_LOGE(TAG, "Pulse creation failed with status %d", status);
        }
    } else {
        ESP_LOGE(TAG, "Pulse creation request failed: %s", esp_err_to_name(err));
    }

    free(post_data);
    esp_http_client_cleanup(client);
    return new_id;
}

void intel_report_otx(const attack_info_t *attack) {
#ifndef CONFIG_OTX_ENABLED
    return;
#else
    if (!CONFIG_OTX_ENABLED || strlen(CONFIG_OTX_API_KEY) == 0) return;
    if (intel_ip_is_private(attack->ip)) return;

    if (cached_pulse_id[0] == '\0') load_pulse_id();

    char *pulse_id = NULL;
    if (cached_pulse_id[0] == '\0') {
        pulse_id = create_pulse(attack);
        if (pulse_id) {
            save_pulse_id(pulse_id);
            free(pulse_id);
            pulse_id = cached_pulse_id;
        } else {
            return;
        }
    } else {
        pulse_id = cached_pulse_id;
    }

    char url[128];
    snprintf(url, sizeof(url), "https://otx.alienvault.com/api/v1/pulses/%s", pulse_id);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_PATCH,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return;

    cJSON *root = cJSON_CreateObject();
    cJSON *indicators = cJSON_AddObjectToObject(root, "indicators");
    cJSON *add = cJSON_AddArrayToObject(indicators, "add");
    cJSON *i = cJSON_CreateObject();
    cJSON_AddStringToObject(i, "type", "IPv4");
    cJSON_AddStringToObject(i, "indicator", attack->ip);
    cJSON_AddStringToObject(i, "role", "bruteforce");
    char title[64];
    snprintf(title, sizeof(title), "%s login attempt", attack->protocol);
    cJSON_AddStringToObject(i, "title", title);
    
    char desc[256];
    snprintf(desc, sizeof(desc), "Brute-force capture. user='%s' proto=%s", attack->user, attack->protocol);
    cJSON_AddStringToObject(i, "description", desc);
    cJSON_AddItemToArray(add, i);

    char *patch_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    esp_http_client_set_header(client, "X-OTX-API-KEY", CONFIG_OTX_API_KEY);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, patch_data, strlen(patch_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            ESP_LOGI(TAG, "Successfully reported %s to OTX pulse %s", attack->ip, pulse_id);
        } else if (status == 404) {
            ESP_LOGW(TAG, "OTX pulse %s not found (404), clearing cache", pulse_id);
            save_pulse_id("");
        } else {
            ESP_LOGE(TAG, "OTX report failed with status %d", status);
        }
    } else {
        ESP_LOGE(TAG, "OTX report request failed: %s", esp_err_to_name(err));
    }

    free(patch_data);
    esp_http_client_cleanup(client);
#endif
}
