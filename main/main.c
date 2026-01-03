
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"

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
#include "SDstorage.h"



// ---------------------------------------------------
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();
    if(!initStorage()){
        return;
    }
    start_webserver();
    ESP_LOGI(TAG, "✅ Ready! Open http://<ESP_IP>/ to upload files to SD.");
}
