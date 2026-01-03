#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "html/html_pages.h"
#include "esp_vfs_fat.h"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

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
