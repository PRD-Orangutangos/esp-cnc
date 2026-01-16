
#include "motor_system.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define TAG "esp-cnc"

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
