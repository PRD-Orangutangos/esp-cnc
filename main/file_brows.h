#pragma once

#include <dirent.h>
#include <sys/stat.h>
#include "esp_http_server.h"
#include <string.h>
#include <dirent.h>
#include "data.h"
static esp_err_t start_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No file specified");
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // завершаем строку

    // Проверка расширения .NC (все заглавные, как на ESP32 FAT)
    const char *dot = strrchr(buf, '.');
    if (!dot || strcmp(dot, ".NC") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file");
        return ESP_FAIL;
    }

    // Сохраняем выбранный файл
    strncpy(selected_gcode_file, buf, sizeof(selected_gcode_file)-1);
    selected_gcode_file[sizeof(selected_gcode_file)-1] = '\0';

    // Запускаем задачу G-code
    begin_read_gcode(); // твоя функция должна использовать selected_gcode_file

    return httpd_resp_sendstr(req, "OK: G-code started");
}

static esp_err_t delete_handler(httpd_req_t *req)
{
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No file specified");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Проверка расширения .NC
    const char *dot = strrchr(buf, '.');
    if (!dot || strcmp(dot, ".NC") != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid file");
        return ESP_FAIL;
    }

    // Формируем полный путь
    char path[132]; // 128 + 4 для "/sd/" и '\0'
    snprintf(path, sizeof(path), "/sd/%s", buf);

    if (remove(path) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }

    return httpd_resp_sendstr(req, "OK: File deleted");
}

static bool is_gcode_file(const char *name)
{
    // Игнорируем точки, служебные файлы и скрытые файлы с '_'
    if (name[0] == '.') return false;
    if (name[0] == '_') return false;
    if (strncmp(name, "LOST.DIR", 8) == 0) return false;
    if (strncmp(name, "SYSTEM~", 7) == 0) return false;

    // Проверяем расширение ".NC" (все заглавные)
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return strcmp(dot, ".NC") == 0;
}

static esp_err_t files_handler(httpd_req_t *req)
{
    DIR *dir = opendir("/sd");
    if (!dir) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open /sd");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/plain");

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (is_gcode_file(entry->d_name)) {
            httpd_resp_sendstr_chunk(req, entry->d_name);
            httpd_resp_sendstr_chunk(req, "\n");
        }
    }

    closedir(dir);
    httpd_resp_sendstr_chunk(req, NULL); // конец chunked ответа
    return ESP_OK;
}



static esp_err_t room_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, file_select_page, HTTPD_RESP_USE_STRLEN);
}

httpd_uri_t root_uri = {
    .uri       = "/lol",
    .method    = HTTP_GET,
    .handler   = room_handler,
    .user_ctx  = NULL
};

httpd_uri_t files_uri = {
    .uri      = "/files",
    .method   = HTTP_GET,
    .handler  = files_handler,
    .user_ctx = NULL
};

httpd_uri_t start_uri = {
    .uri      = "/start",
    .method   = HTTP_POST,
    .handler  = start_handler,
    .user_ctx = NULL
};


httpd_uri_t delete_uri = {
    .uri      = "/delete",
    .method   = HTTP_POST,
    .handler  = delete_handler,
    .user_ctx = NULL
};
