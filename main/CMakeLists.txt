set(requires nvs_flash esp_netif esp_http_server esp_wifi driver )

idf_component_register(
                    SRCS "main.c" 
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES ${requires})