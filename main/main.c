#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_event.h"
#include <esp_http_server.h>
#include "html/html_pages.h"
#include <string.h>



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

// static esp_err_t cmd1_handler(httpd_req_t *req)
// {
//     ESP_LOGI(TAG, "command1!");
//     return httpd_resp_send(req, NULL, 0);
// }

// static esp_err_t cmd2_handler(httpd_req_t *req)
// {
//     ESP_LOGI(TAG, "command2!");
//     return httpd_resp_send(req, NULL, 0);
// }

// static esp_err_t cmd3_handler(httpd_req_t *req)
// {
//     ESP_LOGI(TAG, "command3!");
//     return httpd_resp_send(req, NULL, 0);
// }



// static const httpd_uri_t cmd1 = {
//     .uri       = "/cmd1",
//     .method    = HTTP_GET,
//     .handler   = cmd1_handler,
//     .user_ctx  = NULL,
// };

// static const httpd_uri_t cmd2 = {
//     .uri       = "/cmd2",
//     .method    = HTTP_GET,
//     .handler   = cmd2_handler,
//     .user_ctx  = NULL,
// };

// static const httpd_uri_t cmd3 = {
//     .uri       = "/cmd3",
//     .method    = HTTP_GET,
//     .handler   = cmd3_handler,
//     .user_ctx  = NULL,
// };




// Функция инициализации HTTP + WebSocket сервера
// void start_webserver(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     httpd_handle_t server = NULL;

//     if (httpd_start(&server, &config) == ESP_OK) {
//         ESP_LOGI(TAG, "Web server started");

//         // WebSocket endpoint
//         httpd_uri_t ws_uri = {
//             .uri      = "/ws",
//             .method   = HTTP_GET,
//             .handler  = ws_handler,
//             .user_ctx = NULL,
//             .is_websocket = true
//         };
//         httpd_register_uri_handler(server, &ws_uri);

//         // Обычная главная страница
//         httpd_uri_t main_page_uri = {
//             .uri      = "/",
//             .method   = HTTP_GET,
//             .handler  = [](httpd_req_t *req) {
//                 httpd_resp_send(req, main_page, HTTPD_RESP_USE_STRLEN);
//                 return ESP_OK;
//             },
//             .user_ctx = NULL
//         };
//         httpd_register_uri_handler(server, &main_page_uri);

//         // Обработчики команд (по желанию)
//         // Можно заменить на сообщения через WebSocket
//     }
// }

// static httpd_handle_t start_webserver(void)
// {
//     httpd_handle_t server = NULL;
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     config.lru_purge_enable = true;

//     ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
//     if (httpd_start(&server, &config) == ESP_OK) {
//         ESP_LOGI(TAG, "Registering URI handlers");
//         // httpd_register_uri_handler(server, &any);
//         // httpd_register_uri_handler(server, &cmd1);
//         // httpd_register_uri_handler(server, &cmd2);
//         // httpd_register_uri_handler(server, &cmd3);
//         return server;
//     }

//     ESP_LOGI(TAG, "Error starting server!");
//     return NULL;
// }

// struct async_resp_arg {
//     httpd_handle_t hd;
//     int fd;
// };

/*
 * async send function, which we put into the httpd work queue
 */
// static void ws_async_send(void *arg)
// {
//     static const char * data = "Async data";
//     struct async_resp_arg *resp_arg = arg;
//     httpd_handle_t hd = resp_arg->hd;
//     int fd = resp_arg->fd;
//     httpd_ws_frame_t ws_pkt;
//     memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
//     ws_pkt.payload = (uint8_t*)data;
//     ws_pkt.len = strlen(data);
//     ws_pkt.type = HTTPD_WS_TYPE_TEXT;

//     httpd_ws_send_frame_async(hd, fd, &ws_pkt);
//     free(resp_arg);
// }

// static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
// {
//     struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
//     if (resp_arg == NULL) {
//         return ESP_ERR_NO_MEM;
//     }
//     resp_arg->hd = req->handle;
//     resp_arg->fd = httpd_req_to_sockfd(req);
//     esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
//     if (ret != ESP_OK) {
//         free(resp_arg);
//     }
//     return ret;
// }
static httpd_handle_t server = NULL;
static int ws_client_fd = -1;
static bool ws_connected = false;
static SemaphoreHandle_t ws_mutex = NULL; // Для защиты переменных


void set_ws_client(int fd) {
    xSemaphoreTake(ws_mutex, portMAX_DELAY);
    ws_client_fd = fd;
    ws_connected = true;
    xSemaphoreGive(ws_mutex);
}

// Функция для безопасной очистки при отключении
void clear_ws_client(int fd) {
    xSemaphoreTake(ws_mutex, portMAX_DELAY);
    if (ws_client_fd == fd) {
        ws_connected = false;
        ws_client_fd = -1;
    }
    xSemaphoreGive(ws_mutex);
}

int counter = 0;

void ws_sender_task(void *pvParameter) {
 
    
    while (1) {
        xSemaphoreTake(ws_mutex, portMAX_DELAY);
        bool connected = ws_connected;
        int fd = ws_client_fd;
        xSemaphoreGive(ws_mutex);
        
        if (connected && fd != -1) {
            char data[12];
            snprintf(data, sizeof(data), "%d", counter++);
            
            httpd_ws_frame_t ws_pkt = {
                .type = HTTPD_WS_TYPE_TEXT,
                .len = strlen(data),
                .payload = (uint8_t*)data
            };
            
            // Попытка отправки
            esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ws_pkt);
            
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Соединение разорвано (ошибка %d), очищаем fd", ret);
                xSemaphoreTake(ws_mutex, portMAX_DELAY);
                // Очищаем ТОЛЬКО если fd не изменился (защита от гонок)
                if (ws_client_fd == fd) {
                    ws_connected = false;
                    ws_client_fd = -1;
                }
                xSemaphoreGive(ws_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static esp_err_t ws_handler(httpd_req_t *req) {
    // Новое соединение
    if (req->method == HTTP_GET) {
        xSemaphoreTake(ws_mutex, portMAX_DELAY);
        ws_client_fd = httpd_req_to_sockfd(req);
        ws_connected = true;
        xSemaphoreGive(ws_mutex);
        ESP_LOGI(TAG, "Клиент подключился");
        return ESP_OK;
    }

    // Обработка команд
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // Клиент отключился — получаем ошибку при чтении
        xSemaphoreTake(ws_mutex, portMAX_DELAY);
        ws_connected = false;
        ws_client_fd = -1;
        xSemaphoreGive(ws_mutex);
        ESP_LOGI(TAG, "Клиент отключился");
        return ret;
    }
    
    if (ws_pkt.len) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK && ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            // Обработка команд
             if (strcmp((char*)buf, "cmd1") == 0) {
                ESP_LOGI(TAG, "Команда 1 получена");
                // counter++;
                // char data[12];
                // snprintf(data, sizeof(data), "%d", counter);
                
                // httpd_ws_frame_t ws_pkt = {
                //     .type = HTTPD_WS_TYPE_TEXT,
                //     .len = strlen(data),
                //     .payload = (uint8_t*)data
                // };
                
                // // Попытка отправки
                // esp_err_t ret = httpd_ws_send_frame_async(server, ws_client_fd, &ws_pkt);
                }
            if (strcmp((char*)buf, "cmd2") == 0) {
                ESP_LOGI(TAG, "Команда 2 получена");
                // counter--;
                // char data[12];
                // snprintf(data, sizeof(data), "%d", counter);
                
                // httpd_ws_frame_t ws_pkt = {
                //     .type = HTTPD_WS_TYPE_TEXT,
                //     .len = strlen(data),
                //     .payload = (uint8_t*)data
                // };
                
                // // Попытка отправки
                // esp_err_t ret = httpd_ws_send_frame_async(server, ws_client_fd, &ws_pkt);
            }
            if (strcmp((char*)buf, "cmd3") == 0) {
                ESP_LOGI(TAG, "Команда 3 получена");
            }
        }
        free(buf);
    }
    return ret;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

httpd_uri_t ws_close_uri = {
    .uri = "/ws",
    .method = HTTP_DELETE, // Специальный метод для отключения
    .handler = ws_handler,
    .user_ctx = NULL
};


static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &any);
        ws_mutex = xSemaphoreCreateMutex();
        configASSERT(ws_mutex);
        xTaskCreate(ws_sender_task, "ws_sender", 2048, NULL, 5, NULL);
        httpd_register_uri_handler(server, &ws_close_uri);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void app_main(void)
{


    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    server = start_webserver();

 
}