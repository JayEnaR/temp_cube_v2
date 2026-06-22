#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http-server.h"

#define NVS_NAMESPACE   "storage"
#define NVS_KEY_MODE    "wifi_mode"
#define NVS_KEY_SSID    "wifi_ssid"
#define NVS_KEY_PASS    "wifi_pass"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "http-server";

static httpd_handle_t s_server = NULL;
static wifi_scan_results_t s_scan_results = {0};

extern const char root_html_start[] asm("_binary_root_html_start");
extern const char root_html_end[] asm("_binary_root_html_end");

/* ---------- Small JSON helpers ---------- */

static bool json_escape_string(const char *src, char *dst, size_t dst_len)
{
    size_t pos = 0;

    if (dst_len < 3) {
        return false;
    }

    dst[pos++] = '"';
    for (const unsigned char *p = (const unsigned char *)src; *p != '\0'; p++) {
        const char *escape = NULL;

        switch (*p) {
            case '"': escape = "\\\""; break;
            case '\\': escape = "\\\\"; break;
            case '\b': escape = "\\b"; break;
            case '\f': escape = "\\f"; break;
            case '\n': escape = "\\n"; break;
            case '\r': escape = "\\r"; break;
            case '\t': escape = "\\t"; break;
            default: break;
        }

        if (escape) {
            size_t len = strlen(escape);
            if (pos + len >= dst_len) {
                return false;
            }
            memcpy(dst + pos, escape, len);
            pos += len;
        } else if (*p < 0x20) {
            if (pos + 6 >= dst_len) {
                return false;
            }
            snprintf(dst + pos, dst_len - pos, "\\u%04x", *p);
            pos += 6;
        } else {
            if (pos + 1 >= dst_len) {
                return false;
            }
            dst[pos++] = (char)*p;
        }
    }

    if (pos + 2 > dst_len) {
        return false;
    }
    dst[pos++] = '"';
    dst[pos] = '\0';
    return true;
}

static const char *skip_json_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }
    return p;
}

static bool json_get_string_field(const char *json, const char *key, char *out, size_t out_len)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);
    if (p == NULL || out_len == 0) {
        return false;
    }

    p = strchr(p + strlen(pattern), ':');
    if (p == NULL) {
        return false;
    }

    p = skip_json_ws(p + 1);
    if (*p != '"') {
        return false;
    }
    p++;

    size_t pos = 0;
    while (*p != '\0' && *p != '"') {
        char ch = *p++;

        if (ch == '\\') {
            ch = *p++;
            switch (ch) {
                case '"':
                case '\\':
                case '/':
                    break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default:
                    return false;
            }
        }

        if (pos + 1 >= out_len) {
            return false;
        }
        out[pos++] = ch;
    }

    if (*p != '"') {
        return false;
    }

    out[pos] = '\0';
    return true;
}

/* ---------- Scan results ---------- */

void http_server_set_scan_results(const wifi_scan_results_t *results)
{
    if (results) {
        memcpy(&s_scan_results, results, sizeof(wifi_scan_results_t));
        ESP_LOGI(TAG, "Scan results updated: %d APs", s_scan_results.ap_count);
    }
}

/* ---------- HTTP handlers ---------- */

static esp_err_t handler_root(httpd_req_t *req)
{
    size_t len = root_html_end - root_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_html_start, (ssize_t)len);
    return ESP_OK;
}

static esp_err_t handler_ssid_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    ESP_RETURN_ON_ERROR(httpd_resp_sendstr_chunk(req, "{\"ssids\":["), TAG, "send failed");

    for (int i = 0; i < s_scan_results.ap_count; i++) {
        char escaped[(SSID_MAX_LEN * 6) + 3];

        if (!json_escape_string(s_scan_results.ssids[i], escaped, sizeof(escaped))) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SSID encode failed");
            return ESP_FAIL;
        }

        if (i > 0) {
            ESP_RETURN_ON_ERROR(httpd_resp_sendstr_chunk(req, ","), TAG, "send failed");
        }
        ESP_RETURN_ON_ERROR(httpd_resp_sendstr_chunk(req, escaped), TAG, "send failed");
    }

    ESP_RETURN_ON_ERROR(httpd_resp_sendstr_chunk(req, "]}"), TAG, "send failed");
    return httpd_resp_sendstr_chunk(req, NULL);
}

static esp_err_t handler_ctrl_post(httpd_req_t *req)
{
    char buf[256] = {0};
    int remaining = req->content_len;

    if (remaining >= (int)sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request body too large");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + received,
                                 MIN(remaining, (int)sizeof(buf) - received - 1));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive failed");
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';

    char ssid[SSID_MAX_LEN] = {0};
    char pass[65] = {0};

    if (!json_get_string_field(buf, "ssid", ssid, sizeof(ssid)) || ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or empty ssid");
        return ESP_FAIL;
    }

    json_get_string_field(buf, "password", pass, sizeof(pass));

    ESP_LOGI(TAG, "Credentials received - SSID: %s", ssid);

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, NVS_KEY_SSID, ssid);
        nvs_set_str(h, NVS_KEY_PASS, pass);
        nvs_set_i32(h, NVS_KEY_MODE, (int32_t)WIFI_MODE_STA);
        nvs_commit(h);
        nvs_close(h);
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "OK");

    /* Small delay so the response reaches the client before the restart. */
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();

    return ESP_OK;
}

/* ---------- URI table ---------- */

static const httpd_uri_t uri_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = handler_root,
};

static const httpd_uri_t uri_ssid = {
    .uri = "/ssid",
    .method = HTTP_GET,
    .handler = handler_ssid_get,
};

static const httpd_uri_t uri_ctrl = {
    .uri = "/ctrl",
    .method = HTTP_POST,
    .handler = handler_ctrl_post,
};

/* ---------- Start / stop ---------- */

void start_http_server(void)
{
    if (s_server != NULL) {
        ESP_LOGW(TAG, "Server already running");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting on port %d", config.server_port);
    if (httpd_start(&s_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server");
        return;
    }

    httpd_register_uri_handler(s_server, &uri_root);
    httpd_register_uri_handler(s_server, &uri_ssid);
    httpd_register_uri_handler(s_server, &uri_ctrl);

    ESP_LOGI(TAG, "Server started - connect to http://192.168.4.1");
}

void stop_http_server(void)
{
    if (s_server == NULL) {
        ESP_LOGW(TAG, "Server not running");
        return;
    }
    if (httpd_stop(s_server) == ESP_OK) {
        ESP_LOGI(TAG, "Server stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop server");
    }
    s_server = NULL;
}
