#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include <esp_log.h>
#include "esp_event.h"
#include <esp_http_server.h>
#include "html/html_pages.h"
#include <string.h>
#include "driver/gpio.h"

#define DIR_PIN_X  GPIO_NUM_18
#define STEP_PIN_X GPIO_NUM_19


#define DIR_PIN_Y  GPIO_NUM_20
#define STEP_PIN_Y GPIO_NUM_21


#define DIR_PIN_Z  GPIO_NUM_22
#define STEP_PIN_Z GPIO_NUM_23

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

static int directionX = 0;
static int directionY = 0;
static int directionZ = 0;



static int dirX = 0;
static int dirY = 0;
static int dirZ = 0;

void stepperX(void *pvParameter){
    while(1){
        if(dirX != directionX){
            dirX = directionX;
        }

        if(dirX == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
            // ESP_LOGW(TAG, "X стоп!");
            continue;
        }
        if(dirX == 1){
            // ESP_LOGW(TAG, "X вперед!");
            gpio_set_level(DIR_PIN_X, 1); 
            gpio_set_level(STEP_PIN_X, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_X, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
        if(dirX == 2){
            // ESP_LOGW(TAG, "X назад!");
            gpio_set_level(DIR_PIN_X, 0);
            gpio_set_level(STEP_PIN_X, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_X, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}

void stepperY(void *pvParameter){
    while(1){
        if(dirY != directionY){
            dirY = directionY;
        }

        if(dirY == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
            // ESP_LOGW(TAG, "X стоп!");
            continue;
        }
        if(dirY == 1){
            // ESP_LOGW(TAG, "Y вперед!");
            gpio_set_level(DIR_PIN_Y, 1); 
            gpio_set_level(STEP_PIN_Y, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_Y, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
        if(dirY == 2){
            // ESP_LOGW(TAG, "Y назад!");
            gpio_set_level(DIR_PIN_Y, 0);
            gpio_set_level(STEP_PIN_Y, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_Y, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}

void stepperZ(void *pvParameter){
    while(1){
        if(dirZ != directionZ){
            dirZ = directionZ;
        }

        if(dirZ == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
            // ESP_LOGW(TAG, "X стоп!");
            continue;
        }
        if(dirZ == 1){
            // ESP_LOGW(TAG, "Z вперед!");
            gpio_set_level(DIR_PIN_Z, 1); 
            gpio_set_level(STEP_PIN_Z, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_Z, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
        if(dirZ == 2){
            // ESP_LOGW(TAG, "Z назад!");
            gpio_set_level(DIR_PIN_Z, 0);
            gpio_set_level(STEP_PIN_Z, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEP_PIN_Z, 0);
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req) {
    // Новое соединение
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Клиент подключился");
        return ESP_OK;
    }

    // Обработка команд
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
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
                ESP_LOGI(TAG, "X+ получена");

                directionX = 1;

                }
            if (strcmp((char*)buf, "cmd2") == 0) {
                ESP_LOGI(TAG, "X- получена");

                directionX = 2;


            }
            if (strcmp((char*)buf, "cmd3") == 0) {
                ESP_LOGI(TAG, "X stop получена");

                directionX = 0;

            }
            //Y
            if (strcmp((char*)buf, "cmd4") == 0) {
                ESP_LOGI(TAG, "Y+ получена");

                directionY = 1;

                }
            if (strcmp((char*)buf, "cmd5") == 0) {
                ESP_LOGI(TAG, "Y- получена");

                directionY = 2;


            }
            if (strcmp((char*)buf, "cmd6") == 0) {
                ESP_LOGI(TAG, "Y stop получена");

                directionY = 0;

            }
            //Z
             if (strcmp((char*)buf, "cmd7") == 0) {
                ESP_LOGI(TAG, "Z+ получена");

                directionZ = 1;

                }
            if (strcmp((char*)buf, "cmd8") == 0) {
                ESP_LOGI(TAG, "Z- получена");

                directionZ = 2;


            }
            if (strcmp((char*)buf, "cmd9") == 0) {
                ESP_LOGI(TAG, "Z stop получена");

                directionZ = 0;

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
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &any);
        xTaskCreate(stepperX, "ws_sender", 2048, NULL, 8, NULL);
        xTaskCreate(stepperY, "ws_sender2", 2048, NULL, 8, NULL);
        xTaskCreate(stepperZ, "ws_sender3", 2048, NULL, 8, NULL);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void init_gpio(void)
{
    // Маска для всех 6 пинов (X, Y, Z оси)
    gpio_config_t io_conf = {
        .pin_bit_mask = 
            (1ULL << DIR_PIN_X)  | (1ULL << STEP_PIN_X) |
            (1ULL << DIR_PIN_Y)  | (1ULL << STEP_PIN_Y) |
            (1ULL << DIR_PIN_Z)  | (1ULL << STEP_PIN_Z),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Инициализируем все пины уровнем LOW
    gpio_set_level(DIR_PIN_X, 0);
    gpio_set_level(STEP_PIN_X, 0);
    gpio_set_level(DIR_PIN_Y, 0);
    gpio_set_level(STEP_PIN_Y, 0);
    gpio_set_level(DIR_PIN_Z, 0);
    gpio_set_level(STEP_PIN_Z, 0);
    
    ESP_LOGI(TAG, "GPIO инициализирован: X(DIR=%d,STEP=%d), Y(DIR=%d,STEP=%d), Z(DIR=%d,STEP=%d)", 
             DIR_PIN_X, STEP_PIN_X, DIR_PIN_Y, STEP_PIN_Y, DIR_PIN_Z, STEP_PIN_Z);
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