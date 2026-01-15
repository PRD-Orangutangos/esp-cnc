
// #include "SDstorage.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "lwip/inet.h" 
#include "html/html_pages.h"
#include "motor_system.h"
#include "cJSON.h"

// void app_main(void)P
// {
    

    // if (!initStorage()) {
    //     ESP_LOGE(TAG, "Storage init failed");
    //     return;
    // }

    // init_motors();
    // move_to_base();
    // while(!x_limit_set || !y_limit_set){
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     ESP_LOGW("info:", "now is basing...");
    // }
    // begin_read_gcode();
    // Запускаем G-code асинхронно
    // // Основной цикл свободен
    // while(1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     // get_position();
    //     // ESP_LOGW("pos: ", "%f, %f, %f", current_x_position, current_y_position, current_z_position);
    // }
    
// }




static const char *TAG = "esp-cnc";

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



static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Клиент подключился");
        x_limit_set = false;
        y_limit_set = false;
        z_limit_set = false;
        position_steps_x = 0;
        position_steps_y = 0;
        position_steps_z = 0;
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "Клиент отключился");
        return ret;
    }
    
    if (ws_pkt.len == 0) {
        return ESP_OK;
    }

    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (!buf) return ESP_ERR_NO_MEM;

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(buf);
        return ret;
    }

    // Случай 1: клиент прислал "get_pos" (простая строка)
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len == 7 && strncmp((char*)buf, "get_pos", 7) == 0) {
        get_position();
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", current_x_position);
        cJSON_AddNumberToObject(obj, "y", current_y_position);
        cJSON_AddNumberToObject(obj, "z", current_z_position);
        char *json_str = cJSON_PrintUnformatted(obj);
        cJSON_Delete(obj);

        if (json_str) {
            httpd_ws_frame_t resp = {0};
            resp.payload = (uint8_t*)json_str;
            resp.len = strlen(json_str);
            resp.type = HTTPD_WS_TYPE_TEXT;
            httpd_ws_send_frame(req, &resp);
            free(json_str);
        }
        free(buf);
        return ESP_OK;
    }

    // Случай 2: клиент прислал JSON-команду
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        cJSON *root = cJSON_Parse((char*)buf);
        if (!root) {
            ESP_LOGW(TAG, "Неверный JSON");
            free(buf);
            return ESP_OK;
        }

        cJSON *cmd_obj = cJSON_GetObjectItem(root, "cmd");
        cJSON *step_obj = cJSON_GetObjectItem(root, "step");

        const char *cmd = cmd_obj ? cmd_obj->valuestring : NULL;
        float step = step_obj ? (float)step_obj->valuedouble : 5.0f;

        get_position();

        if (cmd) {
            if (strcmp(cmd, "cmd1") == 0) {
                move_to_position(current_x_position + step, current_y_position, current_z_position);
            } else if (strcmp(cmd, "cmd2") == 0) {
                move_to_position(current_x_position - step, current_y_position, current_z_position);
            } else if (strcmp(cmd, "cmd4") == 0) {
                move_to_position(current_x_position, current_y_position + step, current_z_position);
            } else if (strcmp(cmd, "cmd5") == 0) {
                move_to_position(current_x_position, current_y_position - step, current_z_position);
            } else if (strcmp(cmd, "cmd7") == 0) {
                move_to_position(current_x_position, current_y_position, current_z_position + step);
            } else if (strcmp(cmd, "cmd8") == 0) {
                move_to_position(current_x_position, current_y_position, current_z_position - step);
            }else if (strcmp(cmd, "cmd3") == 0) {
                min_x_position = 0 * STEPS_PER_MM_X;
                position_steps_x = 0;
                current_x_position = 0;
                x_limit_set = true;
            }else if (strcmp(cmd, "cmd6") == 0) {
                min_y_position = 0 * STEPS_PER_MM_Y;
                position_steps_y = 0;
                current_y_position = 0;
                y_limit_set = true;
            }else if (strcmp(cmd, "cmd9") == 0) {
                min_z_position = -5 * STEPS_PER_MM_Z;
                position_steps_z = 0;
                current_z_position = 0;
                z_limit_set = true;
            }
            // stop-команды (cmd3, cmd6, cmd9) — без движения
        }

        cJSON_Delete(root);
    }

    free(buf);
    return ESP_OK;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

// #define WIFI_SSID "Redmi"
// #define WIFI_PASS "12345678"


// static EventGroupHandle_t wifi_event_group;
// const int WIFI_CONNECTED_BIT = BIT0;



// static void wifi_event_handler(void* arg, esp_event_base_t event_base,
//                                int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         ESP_LOGI(TAG, "Disconnected. Reconnecting...");
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         char ip_str[16];
//         ip4_addr_t lwip_ip;
//         lwip_ip.addr = event->ip_info.ip.addr;  // преобразуем esp_ip4_addr_t → ip4_addr_t
//         ip4addr_ntoa_r(&lwip_ip, ip_str, sizeof(ip_str));
//         ESP_LOGI(TAG, "Got IP: %s", ip_str);
//         xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
//     }
// }

// static void wifi_init(void)
// {
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     wifi_event_group = xEventGroupCreate();
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                         ESP_EVENT_ANY_ID,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                         IP_EVENT_STA_GOT_IP,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));

//     wifi_config_t wifi_config = {0};
//     strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
//     strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
//     wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "Wi-Fi initialization started");
//     xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
//     ESP_LOGI(TAG, "Wi-Fi connected!");
// }
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Режим: точка доступа
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // Конфигурация AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32-CNC",
            .ssid_len = 0, // 0 = null-terminated
            .channel = 1,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "ESP32 AP запущена: SSID=ESP32-CNC, пароль=12345678");
}



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

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_motors();

    wifi_init();
    server = start_webserver(); // важно: присваиваем глобальной переменной

    ESP_LOGI(TAG, "✅ Ready! Open http://<ESP_IP>/ to upload files to SD.");
}
