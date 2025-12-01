#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_event.h"
#include <esp_http_server.h>
#include "html/html_pages.h"
#include <time.h>
#include <sys/time.h>
static const char *TAG = "esp-cnc";

static esp_err_t any_handler(httpd_req_t *req)
{
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t any = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = any_handler,
    .user_ctx  = main_page,
};

static esp_err_t cmd1_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "command1!");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t cmd2_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "command2!");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t cmd3_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "command3!");
    return httpd_resp_send(req, NULL, 0);
}



static const httpd_uri_t cmd1 = {
    .uri       = "/cmd1",
    .method    = HTTP_GET,
    .handler   = cmd1_handler,
    .user_ctx  = NULL,
};

static const httpd_uri_t cmd2 = {
    .uri       = "/cmd2",
    .method    = HTTP_GET,
    .handler   = cmd2_handler,
    .user_ctx  = NULL,
};

static const httpd_uri_t cmd3 = {
    .uri       = "/cmd3",
    .method    = HTTP_GET,
    .handler   = cmd3_handler,
    .user_ctx  = NULL,
};

// static esp_err_t sse_handler(httpd_req_t *req)
// {
//     httpd_resp_set_type(req, "text/event-stream");
//     httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
//     httpd_resp_set_hdr(req, "Connection", "keep-alive");

//     char sse_data[64];
//     while (1) {
//         struct timeval tv;
//         gettimeofday(&tv, NULL); // Get the current time
//         int64_t time_since_boot = tv.tv_sec; // Time since boot in seconds
//         esp_err_t err;
//         int len = snprintf(sse_data, sizeof(sse_data), "data: Time since boot: %" PRIi64 " seconds\n\n", time_since_boot);
//         if ((err = httpd_resp_send_chunk(req, sse_data, len)) != ESP_OK) {
//             ESP_LOGE(TAG, "Failed to send sse data (returned %02X)", err);
//             break;
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000)); // Send data every second
//     }

//     httpd_resp_send_chunk(req, NULL, 0); // End response
//     return ESP_OK;
// }
// static const httpd_uri_t sse = {
//     .uri       = "/sse",
//     .method    = HTTP_GET,
//     .handler   = sse_handler,
//     .user_ctx  = NULL
// };

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &any);
        httpd_register_uri_handler(server, &cmd1);
        httpd_register_uri_handler(server, &cmd2);
        httpd_register_uri_handler(server, &cmd3);
        // httpd_register_uri_handler(server, &sse);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void app_main(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    server = start_webserver();

    // while (server) {
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    // }
}