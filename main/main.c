#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_event.h"
#include <esp_http_server.h>
#include "html/html_pages.h"
#include <string.h>
#include "driver/gpio.h"

#define DIR_PIN  GPIO_NUM_18
#define STEP_PIN GPIO_NUM_19

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

static httpd_handle_t server = NULL;
// static int ws_client_fd = -1;
// static bool ws_connected = false;
static int direction = 0;
static SemaphoreHandle_t ws_mutex = NULL; // Для защиты переменных

// static void clear_ws_client(int fd);

// void clear_ws_client(int fd) {
//     xSemaphoreTake(ws_mutex, portMAX_DELAY);
//     if (ws_client_fd == fd) {
//         ws_connected = false;
//         ws_client_fd = -1;
//     }
//     xSemaphoreGive(ws_mutex);
// }

int counter = 0;

// void ws_sender_task(void *pvParameter) {
 
    
//     while (1) {
//         xSemaphoreTake(ws_mutex, portMAX_DELAY);
//         bool connected = ws_connected;
//         int fd = ws_client_fd;
//         xSemaphoreGive(ws_mutex);
        
//         if (connected && fd != -1) {
//             char data[12];
//             snprintf(data, sizeof(data), "%d", counter++);
            
//             httpd_ws_frame_t ws_pkt = {
//                 .type = HTTPD_WS_TYPE_TEXT,
//                 .len = strlen(data),
//                 .payload = (uint8_t*)data
//             };
            
//             // Попытка отправки
//             esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ws_pkt);
//             if (ret != ESP_OK) {
//                 ESP_LOGW(TAG, "Соединение разорвано (ошибка 0x%x)", ret);
//                 clear_ws_client(fd);
//             }

//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
static int dir = 0;
void stepper(void *pvParameter){
    while(1){
        if(dir != direction){
            dir = direction;
        }
        
        // if (xSemaphoreTake(ws_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        //     dir = direction;
        //     xSemaphoreGive(ws_mutex);
        // } else {
        //     ESP_LOGW(TAG, "Семафор занят!");
        //     vTaskDelay(pdMS_TO_TICKS(10));
        // }
        if(dir == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if(dir == 1){
            ESP_LOGW(TAG, "Направление вперед!");
            gpio_set_level(DIR_PIN, 1); 
            gpio_set_level(STEP_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
        if(dir == 2){
            ESP_LOGW(TAG, "Направление назад!");
            gpio_set_level(DIR_PIN, 0);
            gpio_set_level(STEP_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}
static esp_err_t ws_handler(httpd_req_t *req) {
    // Новое соединение
    if (req->method == HTTP_GET) {
        // xSemaphoreTake(ws_mutex, portMAX_DELAY);
        // ws_client_fd = httpd_req_to_sockfd(req);
        // ws_connected = true;
        // xSemaphoreGive(ws_mutex);
        ESP_LOGI(TAG, "Клиент подключился");
        return ESP_OK;
    }

    // Обработка команд
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // Клиент отключился — получаем ошибку при чтении
        // xSemaphoreTake(ws_mutex, portMAX_DELAY);
        // ws_connected = false;
        // ws_client_fd = -1;
        // xSemaphoreGive(ws_mutex);
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

                direction = 1;

                }
            if (strcmp((char*)buf, "cmd2") == 0) {
                ESP_LOGI(TAG, "Команда 2 получена");

                direction = 2;


            }
            if (strcmp((char*)buf, "cmd3") == 0) {
                ESP_LOGI(TAG, "Команда 3 получена");

                direction = 0;

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



static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Creating mutex");
        ws_mutex = xSemaphoreCreateMutex();
        configASSERT(ws_mutex);
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &any);
        xTaskCreate(stepper, "ws_sender", 8192, NULL, 5, NULL);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void init_gpio(void)
{
    // Конфигурируем оба пина одновременно
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DIR_PIN) | (1ULL << STEP_PIN),  // Оба пина
        .mode = GPIO_MODE_OUTPUT,                                  // Режим выхода
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE                            // Без прерываний
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Инициализируем уровнем LOW
    gpio_set_level(DIR_PIN, 0);
    gpio_set_level(STEP_PIN, 0);
    
    ESP_LOGI(TAG, "GPIO инициализирован: DIR=%d, STEP=%d", DIR_PIN, STEP_PIN);
}

void app_main(void)
{

    init_gpio();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    server = start_webserver();

 
}