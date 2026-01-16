#include "esp_wifi.h"
#include "nvs_flash.h"

#define TAG "esp-cnc"

static void wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
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

