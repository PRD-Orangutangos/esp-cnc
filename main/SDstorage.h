#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "html/html_pages.h"
#include "esp_vfs_fat.h"
#include "esp_http_server.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "errno.h"
#include "gcode_parse.h"
#include "axis_system.h"
#define PIN_NUM_MISO 6
#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK  5
#define PIN_NUM_CS   1

#define TAG "SD_POINT"
#define BUF_SIZE 2048

static sdmmc_card_t* mount_sd_card(void)
{
    esp_err_t ret;
    const char mount_point[] = "/sd";
    sdmmc_card_t *card;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 4000;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return NULL;
    }

    ESP_LOGI(TAG, "✅ SD card mounted: %s", card->cid.name);
    return card;
}

bool initStorage(){
    sdmmc_card_t* card = mount_sd_card();
    if (!card) {
        ESP_LOGE(TAG, "SD card initialization failed");
        return false;
    }
    return true;
}


static esp_err_t upload_handler(httpd_req_t *req)
{
    char filepath[128] = "/sd/cnc.bin"; // дефолтное имя на случай ошибки
    char query[128];
    // Получаем имя файла из URL ?name=...
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char name[256] = {0};
        if (httpd_query_key_value(query, "name", name, sizeof(name) - 1) == ESP_OK) {
            // Находим расширение
            const char* dot = strrchr(name, '.');
            char ext[16] = {0};
            if (dot) {
                strncpy(ext, dot, sizeof(ext) - 1);
            }
            // Всегда имя "cnc" + расширение
            snprintf(filepath, sizeof(filepath), "/sd/cnc%s", ext);
        }
    }
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s for writing, errno=%d", filepath, errno);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        return ESP_FAIL;
    }

    static char file_buf[BUF_SIZE];
    setvbuf(f, file_buf, _IOFBF, sizeof(file_buf));

    char *recv_buf = malloc(BUF_SIZE);
    if (!recv_buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Malloc failed");
        return ESP_FAIL;
    }

    int remaining = req->content_len;
    int total_written = 0;

    while (remaining > 0) {
        int to_recv = (remaining > BUF_SIZE) ? BUF_SIZE : remaining;
        int recv_len = httpd_req_recv(req, recv_buf, to_recv);
        if (recv_len <= 0) {
            ESP_LOGE(TAG, "Error receiving data");
            free(recv_buf);
            fclose(f);
            return ESP_FAIL;
        }

        size_t written = fwrite(recv_buf, 1, recv_len, f);
        if (written != recv_len) {
            ESP_LOGE(TAG, "Write error: %d of %d bytes", (int)written, recv_len);
            free(recv_buf);
            fclose(f);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
            return ESP_FAIL;
        }

        total_written += written;
        remaining -= recv_len;
    }

    free(recv_buf);
    fclose(f);

    ESP_LOGI(TAG, "Upload complete: %d bytes written to %s", total_written, filepath);
    return httpd_resp_sendstr(req, "OK: saved");
}

static esp_err_t root_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, upload_page, HTTPD_RESP_USE_STRLEN);
}



void gcode_execution_task(void *arg)
{
    gcode_task_handle = xTaskGetCurrentTaskHandle();  // сохраняем handle

    FILE* f = fopen("/sd/cnc.nc", "r");
    if (!f) {
        ESP_LOGE(TAG, "G-code file not found");
        gcode_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    char line[128];
    gcode_command_t cmd;

    while (fgets(line, sizeof(line), f) != NULL) {

        parse_gcode_line(line, &cmd);

        if (strcmp(cmd.cmd, "COMMENT") == 0) continue;

        if (strcmp(cmd.cmd, "G00") == 0 || strcmp(cmd.cmd, "G01") == 0) {
            float x = cmd.has_x ? cmd.x : x_axis.current_position;
            float y = cmd.has_y ? cmd.y : y_axis.current_position;
            float z = cmd.has_z ? cmd.z : z_axis.current_position;

            ESP_LOGI(TAG, "Move to X:%.3f Y:%.3f Z:%.3f", x, y, z);
            move_to_position(x, y, z);

            // Ждём уведомления от motion_task
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }

    fclose(f);
    gcode_task_handle = NULL;
    ESP_LOGI(TAG, "G-code finished");
    vTaskDelete(NULL);
}

void begin_read_gcode(){
    xTaskCreatePinnedToCore(
        gcode_execution_task,
        "gcode_task",
        4096,
        NULL,
        5,
        NULL,
        0
    );
}

// static httpd_handle_t start_webserver(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     config.stack_size = 16 * 1024;
//     httpd_handle_t server = NULL;

//     ESP_ERROR_CHECK(httpd_start(&server, &config));

//     httpd_uri_t root_uri = {
//         .uri       = "/",
//         .method    = HTTP_GET,
//         .handler   = root_handler,
//         .user_ctx  = NULL
//     };
//     httpd_register_uri_handler(server, &root_uri);

//     httpd_uri_t upload_uri = {
//         .uri       = "/upload",
//         .method    = HTTP_POST,
//         .handler   = upload_handler,
//         .user_ctx  = NULL
//     };
//     httpd_register_uri_handler(server, &upload_uri);

//     ESP_LOGI(TAG, "HTTP server started");
//     return server;
// }
