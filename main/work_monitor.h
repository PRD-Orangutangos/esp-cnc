
#include "axis_system.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define TAG "esp-cnc"

static esp_err_t ws_handler2(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Connected!");
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

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len == 7 && strncmp((char*)buf, "get_pos", 7) == 0) {
        get_position();
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "x", x_axis.current_position);
        cJSON_AddNumberToObject(obj, "y", y_axis.current_position);
        cJSON_AddNumberToObject(obj, "z", z_axis.current_position);
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
            if (strcmp(cmd, "stop") == 0) {
                // ESP_LOGI("loh", "loh stop");
                motion_stop();
            } else if (strcmp(cmd, "pause") == 0) {
                // ESP_LOGI("loh", "loh pause");
                motion_pause();
            }else if (strcmp(cmd, "start") == 0) {
                // ESP_LOGI("loh", "loh start");
                motion_resume();
            }
        }

        cJSON_Delete(root);
    }

    free(buf);
    return ESP_OK;
}

static const httpd_uri_t ws2 = {
        .uri        = "/ws2",
        .method     = HTTP_GET,
        .handler    = ws_handler2,
        .user_ctx   = NULL,
        .is_websocket = true
};
