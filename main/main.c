#include <stdio.h>
#include "protocol_examples_common.h"
#include <nvs_flash.h>
#include "esp_event.h"

void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
}