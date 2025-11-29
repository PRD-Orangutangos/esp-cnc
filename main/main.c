#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_event.h"
#include <esp_http_server.h>


static const char *TAG = "esp-cnc";

static esp_err_t any_handler(httpd_req_t *req)
{
    /* Send response with body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    // End response
    // httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
static const char html_page[] =
"<!DOCTYPE html>"
"<html lang=\"ru\">"
"<head>"
"  <meta charset=\"UTF-8\">"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
"  <title>Управление</title>"
"  <style>"
"    body { font-family: -apple-system, sans-serif; text-align: center; padding: 20px; background: #f5f5f7; }"
"    .container { max-width: 500px; margin: 0 auto; }"
"    #value { font-size: 3rem; font-weight: bold; color: #0071e3; margin: 30px 0; padding: 16px; background: white; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }"
"    .btn { margin: 8px; padding: 12px 24px; font-size: 16px; border: none; border-radius: 8px; cursor: pointer; }"
"    .red { background: #ff3b30; color: white; }"
"    .green { background: #34c759; color: white; }"
"    .blue { background: #0071e3; color: white; }"
"  </style>"
"</head>"
"<body>"
"  <div class=\"container\">"
"    <h2>Данные в реальном времени</h2>"
"    <div id=\"value\">—</div>"
"    <button class=\"btn red\" onclick=\"sendCmd('/cmd1')\">Команда 1</button>"
"    <button class=\"btn green\" onclick=\"sendCmd('/cmd2')\">Команда 2</button>"
"    <button class=\"btn blue\" onclick=\"sendCmd('/cmd3')\">Команда 3</button>"
"  </div>"
""
"  <script>"
"    const valueEl = document.getElementById('value');"
""
"    // Подключаем SSE"
"    const eventSource = new EventSource('/events');"
"    eventSource.onmessage = function(e) {"
"      valueEl.textContent = e.data;"
"    };"
"    eventSource.onerror = function() {"
"      valueEl.textContent = '⚠️ отключено';"
"      eventSource.close();"
"    };"
""
"    // Отправка команд"
"    function sendCmd(url) {"
"      fetch(url, { method: 'GET' })"
"        .then(r => r.text())"
"        .then(msg => console.log('Ответ:', msg))"
"        .catch(err => console.error('Ошибка:', err));"
"    }"
"  </script>"
"</body>"
"</html>";

static const httpd_uri_t any = {
    .uri       = "/",
    .method    = HTTP_ANY,
    .handler   = any_handler,
    .user_ctx  = html_page,
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &any);
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

    while (server) {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}