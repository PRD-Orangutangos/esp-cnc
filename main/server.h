#include "esp_log.h"
#include "esp_http_server.h"
#include "html/html_pages.h"
#include "manual_control.h"
#include "file_brows.h"
#define TAG "esp-cnc"

static httpd_handle_t server = NULL;


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



static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Creating mutex");
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &any);
        
        httpd_register_uri_handler(server, &files_uri);
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &start_uri);
        httpd_register_uri_handler(server, &delete_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}